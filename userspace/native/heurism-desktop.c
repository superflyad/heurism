#define _GNU_SOURCE
/* Heurism workspace and dock: X11/Xft client of the local C control service. */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <json-c/json.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SOCKET "/run/heurism-desktop/control.sock"
#define MAX_HITS 64
#define MAX_TASKS 16
#define MAX_LOCAL_RESULTS 256
#define MAX_LOCAL_SCANNED 2048

enum page { WORKSPACE, MENU, OVERVIEW, SETTINGS, DEVICE, NETWORK, SOUND };
enum action { NONE, SHOW_WORKSPACE, SHOW_MENU, SHOW_SPACES, SHOW_OVERVIEW, SHOW_SETTINGS,
              SHOW_DEVICE, SHOW_NETWORK, SHOW_SOUND, SHOW_POWER, SHOW_QUICK,
              LOCK_DESKTOP, LAUNCH_FILES, LAUNCH_EDITOR, LAUNCH_INSTALLED,
              OPEN_LOCAL,
              LAUNCH_BROWSER, LAUNCH_TERMINAL, LAUNCH_KEYBOARD, TOGGLE_THEME,
              SWITCH_TASK, SWITCH_WORKSPACE, MOVE_WINDOW_WORKSPACE,
              TILE_TASK_LEFT, TILE_TASK_RIGHT,
              MINIMIZE_TASKS, ADMIN_CONSOLE, RESTART_VM, SHUT_DOWN_VM,
              BRIGHTER, DIMMER, TAP_TOGGLE, SCROLL_TOGGLE, SPEED_UP, SPEED_DOWN,
              WIFI_SCAN, WIFI_SELECT, WIFI_CONNECT, WIFI_PASSWORD,
              SOUND_LEFT_UP, SOUND_LEFT_DOWN, SOUND_RIGHT_UP, SOUND_RIGHT_DOWN,
              SOUND_MUTE, BIOS_SELECT, BIOS_NEXT, BIOS_APPLY };
struct hit { Window window; int x, y, width, height; enum action action; int index; };
struct task { Window window; char title[64]; unsigned workspace; int x, y, width, height; };
struct local_item {
    char name[NAME_MAX + 1], detail[128], path[PATH_MAX];
    time_t modified;
    bool directory;
};
struct desktop {
    Display *display;
    int screen, width, height, screen_width, screen_height, dock_x, dock_y, dock_width;
    Window background, dock, panel;
    Visual *visual;
    GC gc;
    XftDraw *background_draw, *dock_draw, *panel_draw;
    XftFont *font_small, *font_body, *font_large;
    Atom type_atom, desktop_atom, dock_atom, state_atom, skip_taskbar_atom;
    Atom strut_atom, client_list_atom, active_atom;
    Atom current_workspace_atom, workspace_count_atom, window_workspace_atom;
    Atom moveresize_atom, supported_atom, workarea_atom;
    Atom frame_extents_atom;
    Atom maximized_horz_atom, maximized_vert_atom;
    struct hit hits[MAX_HITS];
    int hit_count;
    struct task tasks[MAX_TASKS];
    int task_count;
    unsigned workspace_count, current_workspace;
    enum page page;
    bool light;
    char address[64], hostname[128], kernel[128], notice[160];
    long long total_memory, free_memory;
    bool ssh, watch, boot_healthy;
    bool status_ready;
    bool dell, tap, natural_scroll, muted, password_focus;
    int brightness, battery, sound_left, sound_right, network_count, bios_selected, bios_choice;
    double pointer_speed;
    char wifi_state[64], wifi_address[64], wifi_connected[64], wifi_ssid[64], wifi_password[64];
    char networks[12][64];
    struct json_object *bios_items;
    char boot_id[64];
    int published_page;
    bool settings_mode;
    bool power_mode;
    bool launcher_mode;
    bool quick_mode;
    bool spaces_mode;
    unsigned spaces_selected;
    char launcher_query[64];
    int launcher_selected, launcher_offset;
    GList *installed_apps;
    GPtrArray *local_items;
    int local_scanned;
    enum action pending_power;
    time_t power_deadline;
    const char *control_socket;
};

struct launcher_item { const char *name, *detail; enum action action; };
struct launcher_result { const char *name, *detail; enum action action; int index; };
static const struct launcher_item launcher_items[] = {
    {"Files", "Browse and organize", LAUNCH_FILES},
    {"Editor", "Write and revise", LAUNCH_EDITOR},
    {"Browser", "Open the web", LAUNCH_BROWSER},
    {"Terminal", "Run commands", LAUNCH_TERMINAL},
    {"Settings", "Appearance and input", SHOW_SETTINGS},
    {"Spaces", "See open windows and workspaces", SHOW_SPACES},
    {"System", "Machine and services", SHOW_OVERVIEW},
    {"Network", "Connection status", SHOW_NETWORK},
    {"Keyboard", "On-screen input", LAUNCH_KEYBOARD},
    {"Power", "Restart or shut down", SHOW_POWER}
};

static gint compare_app_names(gconstpointer left, gconstpointer right) {
    return g_utf8_collate(g_app_info_get_display_name(G_APP_INFO(left)),
                          g_app_info_get_display_name(G_APP_INFO(right)));
}

static void load_installed_apps(struct desktop *d) {
    GList *all = g_app_info_get_all();
    for (GList *item = all; item; item = item->next) {
        GAppInfo *info = G_APP_INFO(item->data);
        const char *id = g_app_info_get_id(info);
        const char *name = g_app_info_get_display_name(info);
        if (!g_app_info_should_show(info) || !name || !*name ||
            (id && (g_str_has_prefix(id, "heurism-") ||
                    !strcmp(id, "xfce4-session-logout.desktop")))) continue;
        d->installed_apps = g_list_prepend(d->installed_apps, g_object_ref(info));
    }
    g_list_free_full(all, g_object_unref);
    d->installed_apps = g_list_sort(d->installed_apps, compare_app_names);
}

static void scan_local(struct desktop *d, const char *path, const char *place,
                       int depth) {
    if (d->local_items->len >= MAX_LOCAL_RESULTS ||
        d->local_scanned >= MAX_LOCAL_SCANNED) return;
    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) &&
           d->local_items->len < MAX_LOCAL_RESULTS &&
           d->local_scanned < MAX_LOCAL_SCANNED) {
        if (entry->d_name[0] == '.') continue;
        d->local_scanned++;
        char full[PATH_MAX];
        if (snprintf(full, sizeof full, "%s/%s", path, entry->d_name) >=
            (int)sizeof full) continue;
        struct stat info;
        if (fstatat(dirfd(directory), entry->d_name, &info, AT_SYMLINK_NOFOLLOW) ||
            (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode))) continue;
        if (depth >= 0 || S_ISREG(info.st_mode)) {
            struct local_item *item = g_new0(struct local_item, 1);
            snprintf(item->name, sizeof item->name, "%s", entry->d_name);
            snprintf(item->path, sizeof item->path, "%s", full);
            snprintf(item->detail, sizeof item->detail, "%s", place);
            item->directory = S_ISDIR(info.st_mode);
            item->modified = info.st_mtime;
            g_ptr_array_add(d->local_items, item);
        }
        if (depth > 0 && S_ISDIR(info.st_mode)) {
            char child_place[128];
            if (snprintf(child_place, sizeof child_place, "%s/%s", place,
                         entry->d_name) < (int)sizeof child_place)
                scan_local(d, full, child_place, depth - 1);
        }
    }
    closedir(directory);
}

static gint compare_local_recency(gconstpointer left, gconstpointer right) {
    const struct local_item *a = *(const struct local_item * const *)left;
    const struct local_item *b = *(const struct local_item * const *)right;
    if (a->modified != b->modified) return a->modified < b->modified ? 1 : -1;
    return strcmp(a->name, b->name);
}

static void load_local_files(struct desktop *d) {
    const char *home = getenv("HOME");
    d->local_items = g_ptr_array_new_with_free_func(g_free);
    if (!home || home[0] != '/') return;
    scan_local(d, home, "Home", -1);
    const char *places[] = {"Documents", "Downloads", "Desktop", "Pictures"};
    for (size_t i = 0; i < sizeof places / sizeof places[0]; i++) {
        char path[PATH_MAX];
        struct stat info;
        if (snprintf(path, sizeof path, "%s/%s", home, places[i]) >=
            (int)sizeof path || lstat(path, &info) || !S_ISDIR(info.st_mode)) continue;
        scan_local(d, path, places[i], 1);
    }
    g_ptr_array_sort(d->local_items, compare_local_recency);
}

static int (*previous_x_error)(Display *, XErrorEvent *);
static bool shortcut_grab_failed;

static int shortcut_error(Display *display, XErrorEvent *error) {
    if (error->error_code == BadAccess) {
        shortcut_grab_failed = true;
        return 0;
    }
    return previous_x_error ? previous_x_error(display, error) : 0;
}

static unsigned long rgb(struct desktop *d, unsigned red, unsigned green, unsigned blue) {
    unsigned long parts[3] = {red, green, blue};
    unsigned long masks[3] = {d->visual->red_mask, d->visual->green_mask, d->visual->blue_mask};
    unsigned long pixel = 0;
    for (int i = 0; i < 3; i++) {
        unsigned shift = 0;
        unsigned long mask = masks[i];
        if (!mask) continue;
        while (!(mask & 1)) { mask >>= 1; shift++; }
        pixel |= ((parts[i] * mask + 127) / 255) << shift;
    }
    return pixel;
}

static void fill(struct desktop *d, Window window, int x, int y, int width, int height,
                 unsigned red, unsigned green, unsigned blue) {
    if (width <= 0 || height <= 0) return;
    XSetForeground(d->display, d->gc, rgb(d, red, green, blue));
    XFillRectangle(d->display, window, d->gc, x, y, (unsigned)width, (unsigned)height);
}

static void rounded(struct desktop *d, Window window, int x, int y, int width, int height,
                    int radius, unsigned red, unsigned green, unsigned blue) {
    if (width <= radius * 2 || height <= radius * 2) return;
    fill(d, window, x + radius, y, width - radius * 2, height, red, green, blue);
    fill(d, window, x, y + radius, width, height - radius * 2, red, green, blue);
    XSetForeground(d->display, d->gc, rgb(d, red, green, blue));
    int corners[4][2] = {{x, y}, {x + width - radius * 2, y},
                         {x, y + height - radius * 2},
                         {x + width - radius * 2, y + height - radius * 2}};
    for (int i = 0; i < 4; i++)
        XFillArc(d->display, window, d->gc, corners[i][0], corners[i][1],
                 (unsigned)(radius * 2), (unsigned)(radius * 2), 0, 360 * 64);
}

static void label(struct desktop *d, Window window, int x, int y, XftFont *font,
                  const char *text, unsigned red, unsigned green, unsigned blue) {
    bool light_page = d->light && !d->launcher_mode && !d->quick_mode &&
                      !d->spaces_mode &&
                      window == d->background &&
                      (d->settings_mode || d->page != WORKSPACE);
    if (light_page && red == 239 && green == 245 && blue == 255) {
        red = 24; green = 42; blue = 62;
    } else if (light_page && red == 157 && green == 176 && blue == 198) {
        red = 70; green = 91; blue = 112;
    }
    XftDraw *draw = window == d->dock ? d->dock_draw :
                    window == d->panel ? d->panel_draw : d->background_draw;
    XftColor color = {.pixel = rgb(d, red, green, blue),
                      .color = {(unsigned short)(red * 257), (unsigned short)(green * 257),
                                (unsigned short)(blue * 257), 65535}};
    XftDrawStringUtf8(draw, &color, font, x, y, (const FcChar8 *)text, (int)strlen(text));
}

static int label_width(struct desktop *d, XftFont *font, const char *value) {
    XGlyphInfo ink;
    XftTextExtentsUtf8(d->display, font, (const FcChar8 *)value,
                       (int)strlen(value), &ink);
    return ink.xOff;
}

static void hit(struct desktop *d, Window window, int x, int y, int width, int height,
                enum action action, int index) {
    if (d->hit_count >= MAX_HITS) return;
    d->hits[d->hit_count++] = (struct hit){window, x, y, width, height, action, index};
}

static void button(struct desktop *d, Window window, int x, int y, int width, int height,
                   const char *text, enum action action, int index, bool accent) {
    bool light_button = d->light && !d->launcher_mode && !d->quick_mode &&
                        window == d->background &&
                        (d->settings_mode || d->page != WORKSPACE);
    if (accent) rounded(d, window, x, y, width, height, 7, 80, 225, 190);
    else rounded(d, window, x, y, width, height, 7, light_button ? 218 : 29,
                 light_button ? 230 : 48, light_button ? 239 : 65);
    XftFont *font = window == d->dock ? d->font_small : d->font_body;
    label(d, window, x + (window == d->dock ? 10 : 14),
          y + height / 2 + font->ascent / 2 - 2,
          font, text, accent ? 8 : light_button ? 20 : 235,
          accent ? 39 : light_button ? 39 : 243, accent ? 32 : light_button ? 55 : 250);
    hit(d, window, x, y, width, height, action, index);
}

static bool send_request(struct desktop *d, const char *action, const char *value,
                         struct json_object **data) {
    *data = NULL;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (strlen(d->control_socket) >= sizeof address.sun_path) return false;
    strcpy(address.sun_path, d->control_socket);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    if (connect(fd, (struct sockaddr *)&address, sizeof address)) { close(fd); return false; }
    char frame[1024];
    int length = value ? snprintf(frame, sizeof frame,
                                  "{\"version\":1,\"action\":\"%s\",\"value\":%s}\n", action, value) :
                         snprintf(frame, sizeof frame, "{\"version\":1,\"action\":\"%s\"}\n", action);
    if (length < 0 || length >= (int)sizeof frame || write(fd, frame, (size_t)length) != length) {
        close(fd); return false;
    }
    char response[65536];
    size_t used = 0;
    int response_timeout = !strcmp(action, "network-connect") ? 40000 : 12000;
    while (used < sizeof response - 1) {
        struct pollfd input = {.fd = fd, .events = POLLIN};
        if (poll(&input, 1, response_timeout) <= 0) break;
        ssize_t n = read(fd, response + used, 1);
        if (n != 1) break;
        if (response[used++] == '\n') break;
    }
    close(fd);
    if (!used || response[used - 1] != '\n') return false;
    response[used] = 0;
    struct json_object *root = json_tokener_parse(response), *okay = NULL, *item = NULL;
    if (!root || !json_object_object_get_ex(root, "ok", &okay)) {
        if (root) json_object_put(root);
        return false;
    }
    if (!json_object_get_boolean(okay)) {
        if (json_object_object_get_ex(root, "error", &item))
            snprintf(d->notice, sizeof d->notice, "%s", json_object_get_string(item));
        json_object_put(root);
        return false;
    }
    if (json_object_object_get_ex(root, "data", &item) && item) *data = json_object_get(item);
    json_object_put(root);
    return true;
}

static struct json_object *field(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    if (object) json_object_object_get_ex(object, key, &value);
    return value;
}

static void copy_field(char *destination, size_t size, struct json_object *object,
                       const char *key, const char *fallback) {
    struct json_object *value = field(object, key);
    const char *source = value && json_object_get_type(value) == json_type_string ?
                         json_object_get_string(value) : fallback;
    snprintf(destination, size, "%s", source);
}

static void refresh_status(struct desktop *d) {
    struct json_object *data = NULL;
    if (!send_request(d, "status", NULL, &data) || !data) {
        snprintf(d->notice, sizeof d->notice, "Local control service unavailable"); return;
    }
    copy_field(d->hostname, sizeof d->hostname, data, "hostname", "Heurism");
    struct json_object *platform = field(data, "platform");
    d->dell = platform && !strcmp(json_object_get_string(platform), "dell");
    copy_field(d->boot_id, sizeof d->boot_id, data, "boot_id", "");
    d->status_ready = d->boot_id[0] != 0;
    copy_field(d->kernel, sizeof d->kernel, data, "kernel", "Linux");
    struct json_object *network = field(data, "network");
    d->address[0] = 0;
    if (network && json_object_get_type(network) == json_type_array) {
        for (size_t i = 0; i < json_object_array_length(network); i++) {
            struct json_object *entry = json_object_array_get_idx(network, i);
            struct json_object *interface = field(entry, "interface");
            if (interface && !strcmp(json_object_get_string(interface), "eth0")) {
                copy_field(d->address, sizeof d->address, entry, "address", "");
                break;
            }
        }
        if (!d->address[0] && !d->dell && json_object_array_length(network))
            copy_field(d->address, sizeof d->address, json_object_array_get_idx(network, 0),
                       "address", "");
    }
    struct json_object *memory = field(data, "memory");
    d->total_memory = json_object_get_int64(field(memory, "MemTotal"));
    d->free_memory = json_object_get_int64(field(memory, "MemAvailable"));
    struct json_object *management = field(data, "management");
    d->ssh = json_object_get_boolean(field(management, "ssh"));
    d->watch = json_object_get_boolean(field(management, "watch"));
    d->boot_healthy = json_object_get_boolean(field(management, "boot_healthy"));
    d->brightness = json_object_get_int(field(data, "brightness"));
    struct json_object *batteries = field(data, "batteries");
    d->battery = batteries && json_object_get_type(batteries) == json_type_array &&
        json_object_array_length(batteries) ?
        json_object_get_int(field(json_object_array_get_idx(batteries, 0), "capacity")) : -1;
    struct json_object *prefs = field(data, "preferences");
    struct json_object *theme = field(prefs, "theme");
    d->light = theme && !strcmp(json_object_get_string(theme), "light");
    json_object_put(data);
}

static void refresh_page(struct desktop *d) {
    if (!d->dell) return;
    struct json_object *data = NULL;
    if (d->page == SETTINGS && send_request(d, "input-status", NULL, &data) && data) {
        d->tap = json_object_get_boolean(field(data, "tap"));
        d->natural_scroll = json_object_get_boolean(field(data, "natural_scroll"));
        d->pointer_speed = json_object_get_double(field(data, "speed"));
    }
    if (data) { json_object_put(data); data = NULL; }
    if (d->page == SOUND && send_request(d, "sound-status", NULL, &data) && data) {
        d->sound_left = json_object_get_int(field(data, "volume_left"));
        d->sound_right = json_object_get_int(field(data, "volume_right"));
        d->muted = json_object_get_boolean(field(data, "muted"));
    }
    if (data) { json_object_put(data); data = NULL; }
    if (d->page == NETWORK && send_request(d, "network-status", NULL, &data) && data) {
        copy_field(d->wifi_state, sizeof d->wifi_state, data, "state", "Unknown");
        copy_field(d->wifi_address, sizeof d->wifi_address, data, "address", "");
        copy_field(d->wifi_connected, sizeof d->wifi_connected, data, "ssid", "");
    }
    if (data) { json_object_put(data); data = NULL; }
    if (d->page == DEVICE && send_request(d, "bios-list", NULL, &data) && data) {
        if (d->bios_items) json_object_put(d->bios_items);
        d->bios_items = data;
        data = NULL;
    }
    if (data) json_object_put(data);
}

static bool is_shell_window(struct desktop *d, Window window) {
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *property = NULL;
    if (XGetWindowProperty(d->display, window, d->type_atom, 0, 16, False, XA_ATOM,
                           &actual, &format, &count, &remaining, &property) != Success)
        return false;
    bool skip = false;
    if (actual == XA_ATOM && format == 32)
        for (unsigned long i = 0; i < count; i++) {
            Atom type = ((Atom *)property)[i];
            if (type == d->desktop_atom || type == d->dock_atom) skip = true;
        }
    if (property) XFree(property);
    property = NULL;
    if (!skip && XGetWindowProperty(d->display, window, d->state_atom, 0, 16,
                                    False, XA_ATOM, &actual, &format, &count,
                                    &remaining, &property) == Success &&
        actual == XA_ATOM && format == 32)
        for (unsigned long i = 0; i < count; i++)
            if (((Atom *)property)[i] == d->skip_taskbar_atom) skip = true;
    if (property) XFree(property);
    return skip;
}

static bool cardinal_property(struct desktop *d, Window window, Atom property,
                              unsigned long *value) {
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(d->display, window, property, 0, 1, False,
                                    XA_CARDINAL, &actual, &format, &count,
                                    &remaining, &data);
    bool valid = status == Success && actual == XA_CARDINAL && format == 32 &&
                 count == 1 && data;
    if (valid) *value = ((unsigned long *)data)[0];
    if (data) XFree(data);
    return valid;
}

static void refresh_workspaces(struct desktop *d) {
    Window root = RootWindow(d->display, d->screen);
    unsigned long count = 1, current = 0;
    cardinal_property(d, root, d->workspace_count_atom, &count);
    cardinal_property(d, root, d->current_workspace_atom, &current);
    if (count < 1) count = 1;
    if (count > 8) count = 8;
    d->workspace_count = (unsigned)count;
    d->current_workspace = current < count ? (unsigned)current : 0;
}

static void refresh_tasks(struct desktop *d) {
    d->task_count = 0;
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *property = NULL;
    if (XGetWindowProperty(d->display, RootWindow(d->display, d->screen),
                           d->client_list_atom, 0, 256, False, XA_WINDOW,
                           &actual, &format, &count, &remaining, &property) != Success) return;
    if (actual == XA_WINDOW && format == 32) {
        for (unsigned long i = 0; i < count && d->task_count < MAX_TASKS; i++) {
            Window window = ((Window *)property)[i];
            if (window == d->background || window == d->dock ||
                window == d->panel || is_shell_window(d, window)) continue;
            char *title = NULL;
            if (!XFetchName(d->display, window, &title) || !title || !*title) {
                if (title) XFree(title);
                continue;
            }
            struct task *task = &d->tasks[d->task_count++];
            task->window = window;
            snprintf(task->title, sizeof task->title, "%.20s", title);
            unsigned long workspace = d->current_workspace;
            cardinal_property(d, window, d->window_workspace_atom, &workspace);
            task->workspace = (unsigned)workspace;
            XWindowAttributes attributes;
            if (XGetWindowAttributes(d->display, window, &attributes)) {
                Window child;
                task->x = attributes.x;
                task->y = attributes.y;
                XTranslateCoordinates(d->display, window,
                    RootWindow(d->display, d->screen), 0, 0,
                    &task->x, &task->y, &child);
                task->width = attributes.width;
                task->height = attributes.height;
            } else task->x = task->y = task->width = task->height = 0;
            XFree(title);
        }
    }
    if (property) XFree(property);
}

static void dock_symbol(struct desktop *d, int x, enum action action) {
    int left = x + 14, top = 17;
    XSetForeground(d->display, d->gc, rgb(d, 238, 246, 249));
    XSetLineAttributes(d->display, d->gc, 2, LineSolid, CapRound, JoinRound);
    if (action == LAUNCH_FILES) {
        XDrawRectangle(d->display, d->dock, d->gc, left, top + 9, 23, 16);
        XDrawLine(d->display, d->dock, d->gc, left, top + 9, left + 9, top + 9);
        XDrawLine(d->display, d->dock, d->gc, left + 2, top + 5, left + 11, top + 5);
    } else if (action == LAUNCH_EDITOR) {
        XDrawRectangle(d->display, d->dock, d->gc, left + 3, top + 2, 19, 25);
        for (int i = 0; i < 3; i++)
            XDrawLine(d->display, d->dock, d->gc, left + 7, top + 10 + i * 5,
                      left + 18, top + 10 + i * 5);
    } else if (action == LAUNCH_BROWSER) {
        XDrawArc(d->display, d->dock, d->gc, left, top + 2, 25, 25, 0, 360 * 64);
        XDrawArc(d->display, d->dock, d->gc, left + 8, top + 2, 9, 25, 0, 360 * 64);
        XDrawLine(d->display, d->dock, d->gc, left + 1, top + 14,
                  left + 24, top + 14);
    } else if (action == LAUNCH_TERMINAL) {
        XDrawRectangle(d->display, d->dock, d->gc, left, top + 3, 25, 23);
        XDrawLine(d->display, d->dock, d->gc, left + 5, top + 10,
                  left + 10, top + 14);
        XDrawLine(d->display, d->dock, d->gc, left + 10, top + 14,
                  left + 5, top + 18);
        XDrawLine(d->display, d->dock, d->gc, left + 13, top + 19,
                  left + 20, top + 19);
    }
    XSetLineAttributes(d->display, d->gc, 0, LineSolid, CapButt, JoinMiter);
}

static void dock_app(struct desktop *d, int x, const char *name,
                     enum action action, unsigned red, unsigned green, unsigned blue) {
    fill(d, d->dock, x + 5, 8, 42, 42, red, green, blue);
    if (action == SHOW_MENU)
        label(d, d->dock, x + 18, 37, d->font_body, "H", 12, 30, 43);
    else dock_symbol(d, x, action);
    label(d, d->dock, x + 3, 68, d->font_small, name, 200, 214, 224);
    hit(d, d->dock, x, 5, 54, 70, action, 0);
}

static void render_dock(struct desktop *d) {
    fill(d, d->dock, 0, 0, d->dock_width, 78, 19, 28, 43);
    fill(d, d->dock, 1, 1, d->dock_width - 2, 1, 68, 88, 108);
    dock_app(d, 16, "Apps", SHOW_MENU, 75, 219, 194);
    fill(d, d->dock, 79, 14, 1, 48, 73, 91, 109);
    dock_app(d, 91, "Files", LAUNCH_FILES, 49, 91, 143);
    dock_app(d, 151, "Editor", LAUNCH_EDITOR, 129, 79, 77);
    dock_app(d, 211, "Web", LAUNCH_BROWSER, 63, 107, 93);
    dock_app(d, 271, "Terminal", LAUNCH_TERMINAL, 85, 75, 126);
    fill(d, d->dock, 355, 14, 1, 48, 73, 91, 109);
    int x = 368, limit = d->dock_width - 111;
    for (int i = 0; i < d->task_count && x + 56 < limit; i++) {
        int width = (int)strlen(d->tasks[i].title) * 7 + 20;
        if (width > 125) width = 125;
        if (x + width > limit) width = limit - x;
        fill(d, d->dock, x, 15, width, 48, 35, 48, 67);
        label(d, d->dock, x + 9, 45, d->font_small, d->tasks[i].title,
              218, 229, 238);
        hit(d, d->dock, x, 15, width, 48, SWITCH_TASK, i);
        x += width + 6;
    }
    fill(d, d->dock, d->dock_width - 100, 13, 1, 51, 73, 91, 109);
    fill(d, d->dock, d->dock_width - 87, 16, 72, 47, 34, 48, 65);
    label(d, d->dock, d->dock_width - 70, 46, d->font_body, "•••", 235, 243, 248);
    hit(d, d->dock, d->dock_width - 87, 16, 72, 47, SHOW_QUICK, 0);
}

static void render_panel(struct desktop *d) {
    fill(d, d->panel, 0, 0, d->width, 48, 18, 27, 42);
    fill(d, d->panel, 0, 47, d->width, 1, 45, 63, 82);
    fill(d, d->panel, 20, 11, 27, 27, 75, 219, 194);
    label(d, d->panel, 27, 32, d->font_small, "H", 12, 32, 45);
    label(d, d->panel, 59, 32, d->font_body, "Heurism", 237, 244, 249);
    fill(d, d->panel, 159, 15, 1, 20, 75, 91, 107);
    label(d, d->panel, 176, 32, d->font_small, "Spaces", 165, 183, 201);
    label(d, d->panel, 224, 31, d->font_small, "▾", 165, 183, 201);
    hit(d, d->panel, 12, 0, 147, 48, SHOW_MENU, 0);
    hit(d, d->panel, 164, 0, 76, 48, SHOW_SPACES, 0);
    for (unsigned i = 0; i < d->workspace_count; i++) {
        int x = 244 + (int)i * 43;
        bool active = i == d->current_workspace;
        rounded(d, d->panel, x, 9, 36, 30, 7,
                active ? 75 : 35, active ? 219 : 49, active ? 194 : 67);
        char number[3];
        snprintf(number, sizeof number, "%u", i + 1);
        label(d, d->panel, x + 18 - label_width(d, d->font_small, number) / 2,
              27, d->font_small, number,
              active ? 12 : 196, active ? 32 : 212, active ? 45 : 224);
        int occupied = 0;
        for (int task = 0; task < d->task_count; task++)
            if (d->tasks[task].workspace == i ||
                d->tasks[task].workspace == UINT32_MAX) occupied++;
        if (occupied > 3) occupied = 3;
        for (int dot = 0; dot < occupied; dot++)
            fill(d, d->panel, x + 13 + dot * 6, 32, 3, 3,
                 active ? 12 : 111, active ? 32 : 139, active ? 45 : 164);
        hit(d, d->panel, x, 3, 36, 42, SWITCH_WORKSPACE, (int)i);
    }
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char clock_text[64];
    strftime(clock_text, sizeof clock_text, "%a %d %b   %H:%M", &local);
    int clock_x = d->width - label_width(d, d->font_small, clock_text) - 22;
    int network_x = clock_x - 96;
    fill(d, d->panel, network_x, 21, 7, 7,
         d->address[0] ? 75 : 211, d->address[0] ? 219 : 149,
         d->address[0] ? 194 : 127);
    label(d, d->panel, network_x + 17, 32, d->font_small,
          d->address[0] ? "Online" : "Offline", 192, 208, 221);
    label(d, d->panel, clock_x, 32, d->font_small,
          clock_text, 237, 244, 249);
    hit(d, d->panel, d->width - 240, 0, 240, 48, SHOW_QUICK, 0);
}

static void render_workspace(struct desktop *d) {
    for (int y = 0; y < d->height; y += 8) {
        unsigned step = (unsigned)(y * 22 / d->height);
        fill(d, d->background, 0, y, d->width, 8,
             12 + step / 3, 22 + step / 2, 38 + step);
    }
    /* A quiet geometric identity leaves room for real application windows. */
    int cx = d->width * 72 / 100, cy = d->height * 43 / 100;
    for (int i = 0; i < 9; i++) {
        int radius = 380 - i * 30;
        fill(d, d->background, cx - radius, cy - radius / 2,
             radius * 2, 1, 28 + i * 2, 47 + i * 2, 67 + i * 3);
    }
    XPoint facets[4] = {
        {(short)(cx - 210), (short)(cy - 100)},
        {(short)(cx + 110), (short)(cy - 220)},
        {(short)(cx + 260), (short)(cy + 110)},
        {(short)(cx - 60), (short)(cy + 230)}
    };
    XSetForeground(d->display, d->gc, rgb(d, 29, 66, 83));
    XFillPolygon(d->display, d->background, d->gc, facets, 4, Convex, CoordModeOrigin);
    XPoint inset[4] = {
        {(short)(cx - 135), (short)(cy - 55)},
        {(short)(cx + 98), (short)(cy - 145)},
        {(short)(cx + 185), (short)(cy + 65)},
        {(short)(cx - 45), (short)(cy + 155)}
    };
    XSetForeground(d->display, d->gc, rgb(d, 18, 44, 67));
    XFillPolygon(d->display, d->background, d->gc, inset, 4, Convex, CoordModeOrigin);
    fill(d, d->background, cx - 72, cy - 42, 12, 128, 75, 219, 194);
    fill(d, d->background, cx + 50, cy - 77, 12, 128, 75, 219, 194);
    fill(d, d->background, cx - 72, cy + 7, 134, 12, 75, 219, 194);
}

static void render_spaces(struct desktop *d) {
    fill(d, d->background, 0, 0, d->width, d->height, 12, 22, 36);
    fill(d, d->background, 0, 0, d->width, 4, 75, 219, 194);
    label(d, d->background, 36, 48, d->font_small, "H E U R I S M  /  WORKSPACE",
          101, 220, 204);
    label(d, d->background, 36, 103, d->font_large, "Spaces", 239, 245, 255);
    label(d, d->background, 280, 102, d->font_body,
          "Choose a space or open a window", 162, 183, 202);
    int margin = 36, gap = 18, top = 145;
    int card_width = (d->width - 2 * margin - gap) / 2;
    int card_height = (d->height - top - 48 - gap) / 2;
    unsigned visible = d->workspace_count < 4 ? d->workspace_count : 4;
    for (unsigned space = 0; space < visible; space++) {
        int x = margin + (int)(space % 2) * (card_width + gap);
        int y = top + (int)(space / 2) * (card_height + gap);
        bool active = space == d->current_workspace;
        bool selected = space == d->spaces_selected;
        rounded(d, d->background, x, y, card_width, card_height, 12,
                selected ? 51 : 30, selected ? 75 : 46, selected ? 91 : 65);
        if (active) fill(d, d->background, x + 16, y + 17, 4, 26, 75, 219, 194);
        char heading[32], summary[40];
        snprintf(heading, sizeof heading, "Space %u", space + 1);
        label(d, d->background, x + 30, y + 38, d->font_body,
              heading, 239, 245, 255);
        int count = 0;
        for (int task = 0; task < d->task_count; task++)
            if (d->tasks[task].workspace == space ||
                d->tasks[task].workspace == UINT32_MAX) count++;
        snprintf(summary, sizeof summary, "%d window%s%s", count,
                 count == 1 ? "" : "s", active ? "  ·  current" : "");
        label(d, d->background, x + 30, y + 62, d->font_small,
              summary, 169, 191, 208);
        hit(d, d->background, x, y, card_width, card_height,
            SWITCH_WORKSPACE, (int)space);
        int map_x = x + 20, map_y = y + 82;
        int map_width = (card_width - 54) * 55 / 100;
        int map_height = card_height - 105;
        rounded(d, d->background, map_x, map_y, map_width, map_height, 6,
                14, 29, 48);
        int listed = 0;
        for (int task = 0; task < d->task_count; task++) {
            struct task *item = &d->tasks[task];
            if (item->workspace != space && item->workspace != UINT32_MAX) continue;
            if (item->width > 0 && item->height > 0) {
                int window_x = item->x < 0 ? 0 : item->x > d->screen_width ?
                               d->screen_width : item->x;
                int window_y = item->y < 0 ? 0 : item->y > d->screen_height ?
                               d->screen_height : item->y;
                int window_width = item->width > d->screen_width ?
                                   d->screen_width : item->width;
                int window_height = item->height > d->screen_height ?
                                    d->screen_height : item->height;
                int preview_x = map_x + window_x * map_width / d->screen_width;
                int preview_y = map_y + window_y * map_height / d->screen_height;
                int preview_width = window_width * map_width / d->screen_width;
                int preview_height = window_height * map_height / d->screen_height;
                if (preview_x < map_x + 2) preview_x = map_x + 2;
                if (preview_y < map_y + 2) preview_y = map_y + 2;
                if (preview_width < 28) preview_width = 28;
                if (preview_height < 20) preview_height = 20;
                if (preview_x + preview_width > map_x + map_width - 2)
                    preview_width = map_x + map_width - 2 - preview_x;
                if (preview_y + preview_height > map_y + map_height - 2)
                    preview_height = map_y + map_height - 2 - preview_y;
                rounded(d, d->background, preview_x, preview_y,
                        preview_width, preview_height, 3, 68, 102, 123);
                fill(d, d->background, preview_x + 3, preview_y + 3,
                     preview_width - 6, 3, 112, 195, 190);
            }
            if (listed < 4) {
                int list_x = map_x + map_width + 16;
                int list_y = map_y + listed * 33;
                int list_width = x + card_width - 18 - list_x;
                rounded(d, d->background, list_x, list_y,
                        list_width, 29, 5, 41, 62, 80);
                label(d, d->background, list_x + 9, list_y + 20,
                      d->font_small, item->title, 227, 239, 245);
                hit(d, d->background, list_x, list_y,
                    list_width - 62, 29, SWITCH_TASK, task);
                int left_x = list_x + list_width - 59;
                rounded(d, d->background, left_x, list_y + 2,
                        27, 25, 4, 60, 88, 105);
                rounded(d, d->background, left_x + 30, list_y + 2,
                        27, 25, 4, 60, 88, 105);
                label(d, d->background, left_x + 8, list_y + 19,
                      d->font_small, "‹", 235, 246, 250);
                label(d, d->background, left_x + 38, list_y + 19,
                      d->font_small, "›", 235, 246, 250);
                hit(d, d->background, left_x, list_y, 27, 29, TILE_TASK_LEFT, task);
                hit(d, d->background, left_x + 30, list_y, 27, 29,
                    TILE_TASK_RIGHT, task);
                listed++;
            }
        }
        if (!count)
            label(d, d->background, map_x + map_width + 15, map_y + 23,
                  d->font_small, "Ready for work", 140, 163, 182);
    }
    label(d, d->background, 36, d->height - 19, d->font_small,
          "Click a title to focus   ·   ‹ › tile   ·   Esc closes", 145, 171, 190);
}

static const char *bios_name(int index) {
    static const char *names[] = {"FnLock", "FnLockMode", "KeyboardIllumination",
                                  "KbdBacklightTimeoutAc", "KbdBacklightTimeoutBatt"};
    return index >= 0 && index < 5 ? names[index] : NULL;
}

static bool bios_value_at(struct desktop *d, int index, char *out, size_t size) {
    const char *name = bios_name(d->bios_selected);
    struct json_object *item = name ? field(d->bios_items, name) : NULL;
    struct json_object *possible = field(item, "possible_values");
    if (!possible || json_object_get_type(possible) != json_type_string) return false;
    char values[2048];
    snprintf(values, sizeof values, "%s", json_object_get_string(possible));
    char *save = NULL;
    for (char *token = strtok_r(values, ";", &save); token;
         token = strtok_r(NULL, ";", &save), index--)
        if (index == 0) { snprintf(out, size, "%s", token); return true; }
    return false;
}

static int bios_value_count(struct desktop *d) {
    char value[128];
    int count = 0;
    while (count < 64 && bios_value_at(d, count, value, sizeof value)) count++;
    return count;
}

static bool launcher_match(struct desktop *d, const char *name, const char *detail) {
    return !d->launcher_query[0] || strcasestr(name, d->launcher_query) ||
           strcasestr(detail, d->launcher_query);
}

static void launcher_add(int wanted, struct launcher_result *choice, int *count,
                         struct launcher_result result) {
    if (*count == wanted && choice) *choice = result;
    (*count)++;
}

static int launcher_results(struct desktop *d, int wanted, struct launcher_result *choice) {
    int count = 0;
    for (size_t i = 0; i < 4; i++)
        if (launcher_match(d, launcher_items[i].name, launcher_items[i].detail))
            launcher_add(wanted, choice, &count,
                (struct launcher_result){launcher_items[i].name,
                    launcher_items[i].detail, launcher_items[i].action, 0});
    for (int i = 0; i < d->task_count; i++)
        if (launcher_match(d, d->tasks[i].title, "Open window"))
            launcher_add(wanted, choice, &count,
                (struct launcher_result){d->tasks[i].title,
                    "Open window", SWITCH_TASK, i});
    if (strlen(d->launcher_query) >= 2 && d->local_items)
        for (guint i = 0; i < d->local_items->len; i++) {
            struct local_item *item = g_ptr_array_index(d->local_items, i);
            if (strcasestr(item->name, d->launcher_query))
                launcher_add(wanted, choice, &count,
                    (struct launcher_result){item->name, item->detail,
                        OPEN_LOCAL, (int)i});
        }
    for (size_t i = 4; i < sizeof launcher_items / sizeof launcher_items[0]; i++)
        if (launcher_match(d, launcher_items[i].name, launcher_items[i].detail))
            launcher_add(wanted, choice, &count,
                (struct launcher_result){launcher_items[i].name,
                    launcher_items[i].detail, launcher_items[i].action, 0});
    int index = 0;
    for (GList *item = d->installed_apps; item; item = item->next, index++) {
        GAppInfo *info = G_APP_INFO(item->data);
        const char *name = g_app_info_get_display_name(info);
        const char *detail = g_app_info_get_executable(info);
        if (!detail || !*detail) detail = "Installed application";
        if (launcher_match(d, name, detail))
            launcher_add(wanted, choice, &count,
                (struct launcher_result){name, detail, LAUNCH_INSTALLED, index});
    }
    return count;
}

static int launcher_count(struct desktop *d) {
    return launcher_results(d, -1, NULL);
}

static struct launcher_result launcher_choice(struct desktop *d, int selected) {
    struct launcher_result choice = {.action = NONE};
    if (selected >= 0) launcher_results(d, selected, &choice);
    return choice;
}

static void render_launcher(struct desktop *d) {
    fill(d, d->background, 0, 0, d->width, d->height, 20, 29, 44);
    fill(d, d->background, 0, 0, d->width, 1, 73, 94, 113);
    fill(d, d->background, 30, 28, 30, 30, 75, 219, 194);
    label(d, d->background, 37, 52, d->font_small, "H", 12, 30, 43);
    label(d, d->background, 76, 51, d->font_body, "Search Heurism", 237, 244, 249);
    label(d, d->background, d->width - 93, 51, d->font_small,
          "Esc close", 164, 182, 200);
    rounded(d, d->background, 30, 90, d->width - 60, 60, 10, 35, 49, 68);
    char query[96];
    snprintf(query, sizeof query, "%s%s", d->launcher_query[0] ? d->launcher_query :
             "Apps, files, settings and open windows", d->launcher_query[0] ? " |" : "");
    label(d, d->background, 50, 128, d->font_body, query,
          d->launcher_query[0] ? 239 : 157, d->launcher_query[0] ? 245 : 176,
          d->launcher_query[0] ? 255 : 198);
    int count = launcher_count(d);
    int capacity = (d->height - 226) / 54;
    if (capacity < 1) capacity = 1;
    if (d->launcher_selected >= count) d->launcher_selected = count ? count - 1 : 0;
    if (d->launcher_selected < d->launcher_offset) d->launcher_offset = d->launcher_selected;
    if (d->launcher_selected >= d->launcher_offset + capacity)
        d->launcher_offset = d->launcher_selected - capacity + 1;
    if (!count) label(d, d->background, 48, 218, d->font_body,
                      "No matching apps, files or windows", 157, 176, 198);
    for (int index = d->launcher_offset;
         index < count && index < d->launcher_offset + capacity; index++) {
            struct launcher_result item = launcher_choice(d, index);
            int y = 181 + (index - d->launcher_offset) * 54;
            bool selected = index == d->launcher_selected;
            rounded(d, d->background, 30, y, d->width - 60, 48, 7,
                    selected ? 41 : 25, selected ? 67 : 39, selected ? 79 : 57);
            if (selected) fill(d, d->background, 30, y + 9, 3, 30, 77, 211, 194);
            label(d, d->background, 48, y + 22, d->font_body, item.name, 239, 245, 255);
            label(d, d->background, d->width / 2, y + 21, d->font_small,
                  item.detail, 157, 176, 198);
            hit(d, d->background, 30, y, d->width - 60, 48, item.action, item.index);
    }
    char footer[128];
    snprintf(footer, sizeof footer, "%d result%s  ·  ↑↓ choose  ·  Enter open  ·  Esc close",
             count, count == 1 ? "" : "s");
    label(d, d->background, 32, d->height - 25, d->font_small, footer, 157, 176, 198);
}

static void render_quick(struct desktop *d) {
    fill(d, d->background, 0, 0, d->width, d->height, 20, 29, 44);
    fill(d, d->background, 0, 0, d->width, 1, 73, 94, 113);
    label(d, d->background, 25, 49, d->font_body, "Quick settings", 237, 244, 249);
    button(d, d->background, d->width - 85, 19, 61, 39,
           "Close", SHOW_WORKSPACE, 0, false);

    rounded(d, d->background, 24, 82, d->width - 48, 122, 10, 32, 48, 66);
    fill(d, d->background, 42, 106, 8, 8,
         d->address[0] ? 75 : 211, d->address[0] ? 219 : 149,
         d->address[0] ? 194 : 127);
    label(d, d->background, 61, 117, d->font_small,
          d->address[0] ? "Connected" : "Offline", 193, 210, 223);
    label(d, d->background, 42, 152, d->font_body,
          d->address[0] ? d->address : "No Ethernet address", 239, 245, 255);
    label(d, d->background, 42, 182, d->font_small,
          d->dell ? "Speaker controls in Settings" : "Virtual audio; no physical speakers",
          157, 176, 198);

    label(d, d->background, 24, 245, d->font_small,
          "Appearance", 157, 176, 198);
    button(d, d->background, 24, 260, d->width - 48, 51,
           d->light ? "Switch to Night" : "Switch to Light", TOGGLE_THEME, 0, false);
    label(d, d->background, 24, 354, d->font_small,
          "System", 157, 176, 198);
    int half = (d->width - 60) / 2;
    button(d, d->background, 24, 370, half, 49, "Settings", SHOW_SETTINGS, 0, false);
    button(d, d->background, 36 + half, 370, half, 49,
           "Network", SHOW_NETWORK, 0, false);
    button(d, d->background, 24, 435, half, 49, "Lock", LOCK_DESKTOP, 0, false);
    button(d, d->background, 36 + half, 435, half, 49,
           "Power", SHOW_POWER, 0, false);
}

static void render_page(struct desktop *d) {
    if (d->spaces_mode) { render_spaces(d); return; }
    if (d->launcher_mode) { render_launcher(d); return; }
    if (d->quick_mode) { render_quick(d); return; }
    if (d->page == WORKSPACE && !d->settings_mode && !d->power_mode) {
        render_workspace(d);
        return;
    }
    unsigned br = d->light ? 247 : 9, bg = d->light ? 250 : 19, bb = d->light ? 252 : 31;
    fill(d, d->background, 0, 0, d->width, d->height, br, bg, bb);
    XSetForeground(d->display, d->gc, rgb(d, 18, 64, 83));
    XFillArc(d->display, d->background, d->gc, d->width / 2, 90,
             (unsigned)d->width, (unsigned)d->height, 0, 360 * 64);
    if (d->power_mode) {
        label(d, d->background, 48, 72, d->font_body,
              "H E U R I S M", 80, 225, 190);
        label(d, d->background, 48, 133, d->font_large,
              "Power", 239, 245, 255);
        label(d, d->background, 48, 187, d->font_small,
              "Restart or shut down with Heurism's checked power control.",
              157, 176, 198);
        button(d, d->background, 48, 220, 250, 70,
               d->pending_power == RESTART_VM && time(NULL) <= d->power_deadline ?
                   "Confirm restart" : "Restart", RESTART_VM, 0, false);
        button(d, d->background, 322, 220, 250, 70,
               d->pending_power == SHUT_DOWN_VM && time(NULL) <= d->power_deadline ?
                   "Confirm shut down" : "Shut down", SHUT_DOWN_VM, 0, false);
        if (d->pending_power != NONE && time(NULL) <= d->power_deadline)
            label(d, d->background, 48, 325, d->font_small,
                  "Click the same button again within 10 seconds.",
                  157, 176, 198);
        return;
    }
    if (d->page == WORKSPACE) { render_workspace(d); return; }
    const char *title = d->page == MENU ? "Heurism" :
                        d->page == OVERVIEW ? "System overview" :
                        d->page == SETTINGS ? "Appearance and input" :
                        d->page == DEVICE ? "Device information" :
                        d->page == NETWORK ? "Network" : "Sound";
    label(d, d->background, 56, 92, d->font_body, "H E U R I S M", 80, 225, 190);
    label(d, d->background, 56, 164, d->font_large, title, 239, 245, 255);
    if (!d->settings_mode || d->page != MENU)
        button(d, d->background, 58, 195, 190, 44,
               d->settings_mode ? "Heurism menu" : "Back to desktop",
               d->settings_mode ? SHOW_MENU : SHOW_WORKSPACE, 0, false);
    char line[256];
    if (d->page == MENU) {
        const struct { const char *label; enum action action; } pages[] = {
            {d->settings_mode ? "Close Heurism" : "Desktop", SHOW_WORKSPACE},
            {"System overview", SHOW_OVERVIEW},
            {"Settings", SHOW_SETTINGS}, {"Device information", SHOW_DEVICE},
            {"Network", SHOW_NETWORK}, {"On-screen keyboard", LAUNCH_KEYBOARD},
            {"Administrator access", ADMIN_CONSOLE},
            {d->pending_power == RESTART_VM && time(NULL) <= d->power_deadline ?
                "Confirm restart" : "Restart", RESTART_VM},
            {d->pending_power == SHUT_DOWN_VM && time(NULL) <= d->power_deadline ?
                "Confirm shutdown" : "Shut down", SHUT_DOWN_VM},
            {"Sound", SHOW_SOUND}
        };
        for (int i = 0; i < (d->dell ? 10 : 9); i++)
            button(d, d->background, 58 + (i % 2) * 250, 285 + (i / 2) * 82,
                   230, 62, pages[i].label, pages[i].action, 0, false);
    } else if (d->page == OVERVIEW) {
        snprintf(line, sizeof line, "Computer: %s", d->hostname);
        label(d, d->background, 58, 304, d->font_body, line, 239, 245, 255);
        snprintf(line, sizeof line, "Linux kernel: %s", d->kernel);
        label(d, d->background, 58, 350, d->font_body, line, 239, 245, 255);
        snprintf(line, sizeof line, "Available memory: %lld / %lld MiB",
                 d->free_memory / 1048576, d->total_memory / 1048576);
        label(d, d->background, 58, 396, d->font_body, line, 239, 245, 255);
        snprintf(line, sizeof line, "Management: SSH %s  ·  Watch %s  ·  Boot %s",
                 d->ssh ? "ready" : "unavailable", d->watch ? "ready" : "unavailable",
                 d->boot_healthy ? "healthy" : "unconfirmed");
        label(d, d->background, 58, 442, d->font_body, line, 239, 245, 255);
        if (d->dell) {
            snprintf(line, sizeof line, "Battery: %s", d->battery >= 0 ? "present" : "unavailable");
            if (d->battery >= 0) snprintf(line, sizeof line, "Battery: %d%%", d->battery);
            label(d, d->background, 58, 488, d->font_body, line, 157, 176, 198);
            button(d, d->background, 58, 530, 190, 52, "Speaker controls", SHOW_SOUND, 0, false);
        } else label(d, d->background, 58, 488, d->font_body,
                     "Virtual audio · no physical speakers", 157, 176, 198);
    } else if (d->page == SETTINGS) {
        snprintf(line, sizeof line, "Appearance: %s", d->light ? "Light" : "Night");
        label(d, d->background, 58, 310, d->font_body, line, 239, 245, 255);
        button(d, d->background, 58, 338, 225, 52, "Change appearance", TOGGLE_THEME, 0, true);
        if (d->dell) {
            snprintf(line, sizeof line, "Brightness: %d%%", d->brightness);
            label(d, d->background, 58, 445, d->font_body, line, 239, 245, 255);
            button(d, d->background, 58, 468, 110, 48, "Dim", DIMMER, 0, false);
            button(d, d->background, 180, 468, 125, 48, "Brighten", BRIGHTER, 0, false);
            snprintf(line, sizeof line, "Touchpad tap: %s", d->tap ? "On" : "Off");
            button(d, d->background, 58, 550, 260, 48, line, TAP_TOGGLE, 0, false);
            snprintf(line, sizeof line, "Natural scroll: %s", d->natural_scroll ? "On" : "Off");
            button(d, d->background, 330, 550, 300, 48, line, SCROLL_TOGGLE, 0, false);
            snprintf(line, sizeof line, "Pointer speed: %.1f", d->pointer_speed);
            label(d, d->background, 58, 670, d->font_body, line, 239, 245, 255);
            button(d, d->background, 58, 695, 120, 48, "Slower", SPEED_DOWN, 0, false);
            button(d, d->background, 190, 695, 120, 48, "Faster", SPEED_UP, 0, false);
        } else {
            label(d, d->background, 58, 440, d->font_body,
                  "Virtual keyboard and pointer active", 239, 245, 255);
            label(d, d->background, 58, 478, d->font_small,
                  "Physical touchpad settings are unavailable on this VM", 157, 176, 198);
        }
    } else if (d->page == DEVICE) {
        if (d->dell) {
            label(d, d->background, 58, 300, d->font_body,
                  "Dell Inspiron 7506 2n1 · BIOS attributes", 239, 245, 255);
            for (int i = 0; i < 5; i++) {
                const char *name = bios_name(i);
                struct json_object *item = field(d->bios_items, name);
                copy_field(line, sizeof line, item, "current_value", "Unavailable");
                char entry[320];
                snprintf(entry, sizeof entry, "%s: %s", name, line);
                button(d, d->background, 58, 335 + i * 58, 480, 48, entry,
                       BIOS_SELECT, i, i == d->bios_selected);
            }
            char choice[128];
            if (d->bios_choice >= 0 && bios_value_at(d, d->bios_choice, choice, sizeof choice))
                snprintf(line, sizeof line, "Selected value: %s", choice);
            else snprintf(line, sizeof line, "Select a value for %s", bios_name(d->bios_selected));
            label(d, d->background, 58, 675, d->font_body, line, 239, 245, 255);
            button(d, d->background, 58, 705, 180, 48, "Next value", BIOS_NEXT, 0, false);
            button(d, d->background, 250, 705, 180, 48, "Apply value", BIOS_APPLY, 0, true);
        } else {
            label(d, d->background, 58, 310, d->font_body,
                  "Microsoft Hyper-V virtual machine", 239, 245, 255);
            label(d, d->background, 58, 354, d->font_body,
                  "Dell BIOS controls are unavailable here", 157, 176, 198);
        }
    } else if (d->page == NETWORK) {
        snprintf(line, sizeof line, "Ethernet: %s", d->address[0] ? d->address : "Offline");
        label(d, d->background, 58, 310, d->font_body, line, 239, 245, 255);
        if (d->dell) {
            snprintf(line, sizeof line, "Wi-Fi: %s  %.32s  %s", d->wifi_state,
                     d->wifi_connected, d->wifi_address[0] ? d->wifi_address : "");
            label(d, d->background, 58, 352, d->font_body, line, 239, 245, 255);
            button(d, d->background, 58, 380, 180, 48, "Scan networks", WIFI_SCAN, 0, false);
            for (int i = 0; i < d->network_count && i < 8; i++)
                button(d, d->background, 58 + (i % 2) * 335, 445 + (i / 2) * 55,
                       320, 46, d->networks[i], WIFI_SELECT, i, false);
            snprintf(line, sizeof line, "Name: %.32s", d->wifi_ssid);
            label(d, d->background, 58, 695, d->font_body, line, 239, 245, 255);
            char masked[64];
            size_t length = strlen(d->wifi_password);
            if (length >= sizeof masked) length = sizeof masked - 1;
            memset(masked, '*', length);
            masked[length] = 0;
            snprintf(line, sizeof line, "Password: %s%s", masked,
                     d->password_focus ? " |" : "");
            button(d, d->background, 58, 712, 530, 46, line, WIFI_PASSWORD, 0, false);
            button(d, d->background, 600, 712, 190, 46, "Connect", WIFI_CONNECT, 0, true);
            label(d, d->background, 58, 795, d->font_small,
                  "Select a network, enter its WPA password, then Connect.", 157, 176, 198);
        } else label(d, d->background, 58, 354, d->font_body,
                     "No wireless interface is present on this VM", 157, 176, 198);
    } else if (d->page == SOUND && d->dell) {
        snprintf(line, sizeof line, "Left speaker: %d%%", d->sound_left);
        label(d, d->background, 58, 310, d->font_body, line, 239, 245, 255);
        button(d, d->background, 58, 340, 160, 52, "Quieter", SOUND_LEFT_DOWN, 0, false);
        button(d, d->background, 232, 340, 160, 52, "Louder", SOUND_LEFT_UP, 0, false);
        snprintf(line, sizeof line, "Right speaker: %d%%", d->sound_right);
        label(d, d->background, 58, 445, d->font_body, line, 239, 245, 255);
        button(d, d->background, 58, 475, 160, 52, "Quieter", SOUND_RIGHT_DOWN, 0, false);
        button(d, d->background, 232, 475, 160, 52, "Louder", SOUND_RIGHT_UP, 0, false);
        snprintf(line, sizeof line, "Speaker: %s", d->muted ? "Muted" : "On");
        button(d, d->background, 58, 565, 230, 52, line, SOUND_MUTE, 0, true);
    }
}

static void redraw(struct desktop *d) {
    d->hit_count = 0;
    render_page(d);
    if (d->notice[0]) label(d, d->background, 58,
                             d->power_mode || d->quick_mode || d->spaces_mode ?
                                 d->height - 28 : d->height - 155,
                             d->font_small,
                             d->notice, 80, 225, 190);
    if (!d->settings_mode) {
        render_panel(d);
        render_dock(d);
    }
    XFlush(d->display);
}

static bool ensure_visible(struct desktop *d) {
    XWindowAttributes background, dock, panel;
    if (!XGetWindowAttributes(d->display, d->background, &background) ||
        !XGetWindowAttributes(d->display, d->dock, &dock) ||
        !XGetWindowAttributes(d->display, d->panel, &panel)) return false;
    bool remapped = background.map_state != IsViewable ||
                    dock.map_state != IsViewable || panel.map_state != IsViewable;
    if (background.map_state != IsViewable) XMapWindow(d->display, d->background);
    if (dock.map_state != IsViewable) XMapWindow(d->display, d->dock);
    if (panel.map_state != IsViewable) XMapWindow(d->display, d->panel);
    if (remapped) {
        XLowerWindow(d->display, d->background);
        XRaiseWindow(d->display, d->dock);
        XRaiseWindow(d->display, d->panel);
    }
    Window child;
    int x, y;
    Window root = RootWindow(d->display, d->screen);
    if (!XTranslateCoordinates(d->display, d->dock, root, 0, 0, &x, &y, &child))
        return false;
    if (x != d->dock_x || y != d->dock_y)
        XMoveWindow(d->display, d->dock, d->dock_x, d->dock_y);
    XSync(d->display, False);
    if (!XTranslateCoordinates(d->display, d->dock, root, 0, 0, &x, &y, &child) ||
        x != d->dock_x || y != d->dock_y) return false;
    return XGetWindowAttributes(d->display, d->background, &background) &&
           XGetWindowAttributes(d->display, d->dock, &dock) &&
           XGetWindowAttributes(d->display, d->panel, &panel) &&
           background.map_state == IsViewable && dock.map_state == IsViewable &&
           panel.map_state == IsViewable;
}

static void publish_health(struct desktop *d) {
    if (!d->status_ready || d->published_page == (int)d->page ||
        strcmp(DisplayString(d->display), ":0") || !ensure_visible(d)) return;
    const char *home = getenv("HOME");
    if (!home || *home != '/') return;
    char executable[512], path[1024], temporary[1040];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof executable - 1);
    if (length <= 0 || length >= (ssize_t)sizeof executable - 1) return;
    executable[length] = 0;
    char *slash = strrchr(executable, '/');
    if (!slash) return;
    *slash = 0;
    if (snprintf(path, sizeof path, "%s/.local/state/heurism/session-health-native.json", home) >= (int)sizeof path ||
        snprintf(temporary, sizeof temporary, "%s.new.XXXXXX", path) >= (int)sizeof temporary) return;
    struct json_object *record = json_object_new_object();
    json_object_object_add(record, "pid", json_object_new_int((int)getpid()));
    json_object_object_add(record, "uid", json_object_new_int((int)getuid()));
    json_object_object_add(record, "release", json_object_new_string(executable));
    json_object_object_add(record, "boot_id", json_object_new_string(d->boot_id));
    json_object_object_add(record, "version", json_object_new_string("native-0.8"));
    const char *pages[] = {"workspace", "menu", "overview", "settings", "device", "network", "sound"};
    _Static_assert(sizeof pages / sizeof pages[0] == SOUND + 1,
                   "every desktop page needs a health name");
    json_object_object_add(record, "page", json_object_new_string(pages[d->page]));
    const char *data = json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN);
    int fd = mkstemp(temporary);
    if (fd >= 0) {
        size_t size = strlen(data);
        bool okay = fchmod(fd, 0600) == 0 && write(fd, data, size) == (ssize_t)size &&
                    write(fd, "\n", 1) == 1 && fsync(fd) == 0;
        if (close(fd)) okay = false;
        if (okay) okay = rename(temporary, path) == 0;
        if (okay) d->published_page = d->page;
        else unlink(temporary);
    }
    json_object_put(record);
}

static void launch(struct desktop *d, const char *path, const char *argument) {
    if (access(path, X_OK)) {
        snprintf(d->notice, sizeof d->notice, "Application is not installed yet"); return;
    }
    pid_t pid = fork();
    if (pid < 0) { snprintf(d->notice, sizeof d->notice, "Could not start application"); return; }
    if (!pid) {
        setsid();
        if (argument) execl(path, path, argument, (char *)NULL);
        else execl(path, path, (char *)NULL);
        _exit(127);
    }
    d->notice[0] = 0;
}

static void launch_native_at(struct desktop *d, const char *name,
                             const char *argument) {
    char path[512];
    ssize_t length = readlink("/proc/self/exe", path, sizeof path - 1);
    if (length <= 0 || length >= (ssize_t)sizeof path - 1) return;
    path[length] = 0;
    char *slash = strrchr(path, '/');
    if (!slash || (size_t)(slash - path) + 1 + strlen(name) >= sizeof path) return;
    strcpy(slash + 1, name);
    launch(d, path, argument);
}

static void launch_native(struct desktop *d, const char *name) {
    launch_native_at(d, name, NULL);
}

static void open_local(struct desktop *d, int index) {
    if (!d->local_items || index < 0 || (guint)index >= d->local_items->len) return;
    struct local_item *item = g_ptr_array_index(d->local_items, (guint)index);
    if (item->directory) {
        launch_native_at(d, "heurism-files", item->path);
        return;
    }
    char *type = g_content_type_guess(item->path, NULL, 0, NULL);
    bool text_file = type && g_content_type_is_a(type, "text/plain");
    g_free(type);
    if (text_file) {
        launch_native_at(d, "heurism-editor", item->path);
        return;
    }
    GFile *file = g_file_new_for_path(item->path);
    char *uri = g_file_get_uri(file);
    GError *error = NULL;
    if (!uri || !g_app_info_launch_default_for_uri(uri, NULL, &error))
        snprintf(d->notice, sizeof d->notice, "Could not open file: %s",
                 error ? error->message : "no application registered");
    g_clear_error(&error);
    g_free(uri);
    g_object_unref(file);
}

static void launch_self(struct desktop *d, const char *argument) {
    char path[512];
    ssize_t length = readlink("/proc/self/exe", path, sizeof path - 1);
    if (length <= 0 || length >= (ssize_t)sizeof path - 1) {
        snprintf(d->notice, sizeof d->notice, "Could not find Heurism desktop");
        return;
    }
    path[length] = 0;
    launch(d, path, argument);
}

static void switch_workspace(struct desktop *d, int index);

static void activate_window(struct desktop *d, Window window) {
    unsigned long workspace;
    if (cardinal_property(d, window, d->window_workspace_atom, &workspace) &&
        workspace < d->workspace_count && workspace != d->current_workspace)
        switch_workspace(d, (int)workspace);
    XMapRaised(d->display, window);
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = d->active_atom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2;
    XSendEvent(d->display, RootWindow(d->display, d->screen), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

static void activate_task(struct desktop *d, int index) {
    if (index >= 0 && index < d->task_count)
        activate_window(d, d->tasks[index].window);
}

static bool wm_supports(struct desktop *d, Atom feature) {
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    bool found = false;
    if (XGetWindowProperty(d->display, RootWindow(d->display, d->screen),
                           d->supported_atom, 0, 256, False, XA_ATOM,
                           &actual, &format, &count, &remaining, &data) == Success &&
        actual == XA_ATOM && format == 32 && data)
        for (unsigned long i = 0; i < count; i++)
            if (((Atom *)data)[i] == feature) found = true;
    if (data) XFree(data);
    return found;
}

static bool workspace_area(struct desktop *d, unsigned workspace,
                           int *x, int *y, int *width, int *height) {
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    bool valid = false;
    if (XGetWindowProperty(d->display, RootWindow(d->display, d->screen),
                           d->workarea_atom, 0, 32, False, XA_CARDINAL,
                           &actual, &format, &count, &remaining, &data) == Success &&
        actual == XA_CARDINAL && format == 32 &&
        count >= 4UL * (workspace + 1) && data) {
        unsigned long *area = ((unsigned long *)data) + 4 * workspace;
        if (area[0] < (unsigned long)d->screen_width &&
            area[1] < (unsigned long)d->screen_height &&
            area[2] > 100 && area[2] <= (unsigned long)d->screen_width &&
            area[3] > 100 && area[3] <= (unsigned long)d->screen_height &&
            area[0] + area[2] <= (unsigned long)d->screen_width &&
            area[1] + area[3] <= (unsigned long)d->screen_height) {
            *x = (int)area[0]; *y = (int)area[1];
            *width = (int)area[2]; *height = (int)area[3];
            valid = true;
        }
    }
    if (data) XFree(data);
    return valid;
}

static bool window_frame_extents(struct desktop *d, Window window,
                                 int *left, int *right, int *top, int *bottom) {
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    bool valid = false;
    if (XGetWindowProperty(d->display, window, d->frame_extents_atom,
                           0, 4, False, XA_CARDINAL, &actual, &format,
                           &count, &remaining, &data) == Success &&
        actual == XA_CARDINAL && format == 32 && count == 4 && data) {
        unsigned long *edges = (unsigned long *)data;
        if (edges[0] <= 128 && edges[1] <= 128 &&
            edges[2] <= 128 && edges[3] <= 128) {
            *left = (int)edges[0]; *right = (int)edges[1];
            *top = (int)edges[2]; *bottom = (int)edges[3];
            valid = true;
        }
    }
    if (data) XFree(data);
    return valid;
}

static bool tile_task(struct desktop *d, int index, bool right) {
    if (index < 0 || index >= d->task_count ||
        !wm_supports(d, d->moveresize_atom)) {
        snprintf(d->notice, sizeof d->notice, "Window tiling is unavailable");
        return false;
    }
    struct task *task = &d->tasks[index];
    unsigned workspace = task->workspace == UINT32_MAX ?
                         d->current_workspace : task->workspace;
    int area_x, area_y, area_width, area_height;
    if (workspace >= d->workspace_count ||
        !workspace_area(d, workspace, &area_x, &area_y,
                        &area_width, &area_height)) {
        snprintf(d->notice, sizeof d->notice, "Window area is unavailable");
        return false;
    }
    int left, edge_right, top, bottom;
    if (!window_frame_extents(d, task->window,
                              &left, &edge_right, &top, &bottom)) {
        snprintf(d->notice, sizeof d->notice, "Window frame is unavailable");
        return false;
    }
    int margin = 12, gap = 12;
    int half_width = (area_width - margin * 2 - gap) / 2;
    int frame_height = area_height - margin * 2;
    int target_width = half_width - left - edge_right;
    int target_height = frame_height - top - bottom;
    if (target_width < 100 || target_height < 100) {
        snprintf(d->notice, sizeof d->notice, "Window area is too small");
        return false;
    }
    XSizeHints hints;
    long supplied;
    if (XGetWMNormalHints(d->display, task->window, &hints, &supplied) &&
        (hints.flags & PMinSize) &&
        (hints.min_width > target_width || hints.min_height > target_height)) {
        snprintf(d->notice, sizeof d->notice,
                 "This window needs more room than half the screen");
        return false;
    }
    Window root = RootWindow(d->display, d->screen);
    XEvent state = {0};
    state.xclient.type = ClientMessage;
    state.xclient.window = task->window;
    state.xclient.message_type = d->state_atom;
    state.xclient.format = 32;
    state.xclient.data.l[0] = 0;
    state.xclient.data.l[1] = (long)d->maximized_horz_atom;
    state.xclient.data.l[2] = (long)d->maximized_vert_atom;
    state.xclient.data.l[3] = 2;
    XSendEvent(d->display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &state);
    XEvent move = {0};
    move.xclient.type = ClientMessage;
    move.xclient.window = task->window;
    move.xclient.message_type = d->moveresize_atom;
    move.xclient.format = 32;
    move.xclient.data.l[0] = StaticGravity | (15L << 8) | (2L << 12);
    move.xclient.data.l[1] = area_x + margin + (right ? half_width + gap : 0) + left;
    move.xclient.data.l[2] = area_y + margin + top;
    move.xclient.data.l[3] = target_width;
    move.xclient.data.l[4] = target_height;
    if (!XSendEvent(d->display, root, False,
                    SubstructureRedirectMask | SubstructureNotifyMask, &move)) {
        snprintf(d->notice, sizeof d->notice, "Window tiling request failed");
        return false;
    }
    activate_window(d, task->window);
    XSync(d->display, False);
    return true;
}

static void switch_workspace(struct desktop *d, int index) {
    if (index < 0 || (unsigned)index >= d->workspace_count) return;
    Window root = RootWindow(d->display, d->screen);
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = root;
    event.xclient.message_type = d->current_workspace_atom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = index;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(d->display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(d->display);
}

static void move_active_window(struct desktop *d, int index) {
    if (index < 0 || (unsigned)index >= d->workspace_count) return;
    Window root = RootWindow(d->display, d->screen);
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    if (XGetWindowProperty(d->display, root, d->active_atom, 0, 1, False,
                           XA_WINDOW, &actual, &format, &count, &remaining,
                           &data) != Success || actual != XA_WINDOW ||
        format != 32 || count != 1 || !data) {
        if (data) XFree(data);
        return;
    }
    Window window = ((Window *)data)[0];
    XFree(data);
    if (!window || is_shell_window(d, window)) return;
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = d->window_workspace_atom;
    event.xclient.format = 32;
    event.xclient.data.l[0] = index;
    event.xclient.data.l[1] = 2;
    XSendEvent(d->display, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    switch_workspace(d, index);
}

static void open_panel(struct desktop *d, const char *selection_name, const char *argument) {
    Atom selection = XInternAtom(d->display, selection_name, False);
    Window owner = XGetSelectionOwner(d->display, selection);
    XWindowAttributes attributes;
    if (owner && XGetWindowAttributes(d->display, owner, &attributes) &&
        attributes.map_state == IsViewable) activate_window(d, owner);
    else launch_self(d, argument);
}

static void run_action(struct desktop *d, enum action action, int index) {
    if (d->spaces_mode) {
        if (action == SWITCH_WORKSPACE) switch_workspace(d, index);
        else if (action == SWITCH_TASK) activate_task(d, index);
        else if (action == TILE_TASK_LEFT || action == TILE_TASK_RIGHT) {
            if (!tile_task(d, index, action == TILE_TASK_RIGHT)) {
                redraw(d);
                return;
            }
        }
        else if (action != SHOW_WORKSPACE) return;
        XCloseDisplay(d->display);
        exit(0);
    }
    if (d->quick_mode) {
        if (action == SHOW_WORKSPACE) { XCloseDisplay(d->display); exit(0); }
        const char *argument = action == SHOW_SETTINGS ? "--settings" :
                               action == SHOW_NETWORK ? "--network" :
                               action == SHOW_POWER ? "--power" : NULL;
        if (argument || action == LOCK_DESKTOP) {
            d->notice[0] = 0;
            if (argument) launch_self(d, argument);
            else launch(d, "/usr/bin/xfce4-screensaver-command", "--lock");
            if (!d->notice[0]) { XCloseDisplay(d->display); exit(0); }
            redraw(d);
            return;
        }
    }
    if (d->launcher_mode) {
        d->notice[0] = 0;
        switch (action) {
        case LAUNCH_FILES: launch_native(d, "heurism-files"); break;
        case LAUNCH_EDITOR: launch_native(d, "heurism-editor"); break;
        case LAUNCH_BROWSER: launch(d, "/usr/bin/firefox", NULL); break;
        case LAUNCH_TERMINAL: launch_native(d, "heurism-terminal"); break;
        case LAUNCH_KEYBOARD: launch(d, "/usr/bin/onboard", NULL); break;
        case LAUNCH_INSTALLED: {
            GAppInfo *info = G_APP_INFO(g_list_nth_data(d->installed_apps, (guint)index));
            GError *error = NULL;
            if (!info || !g_app_info_launch(info, NULL, NULL, &error))
                snprintf(d->notice, sizeof d->notice, "Could not start application: %s",
                         error ? error->message : "unavailable");
            if (error) g_error_free(error);
            break;
        }
        case OPEN_LOCAL: open_local(d, index); break;
        case SHOW_SETTINGS: launch_self(d, "--settings"); break;
        case SHOW_SPACES: launch_self(d, "--spaces"); break;
        case SHOW_OVERVIEW: launch_self(d, "--overview"); break;
        case SHOW_NETWORK: launch_self(d, "--network"); break;
        case SHOW_POWER: launch_self(d, "--power"); break;
        case SWITCH_TASK: activate_task(d, index); break;
        case SHOW_WORKSPACE: XCloseDisplay(d->display); exit(0);
        default: return;
        }
        if (!d->notice[0]) { XCloseDisplay(d->display); exit(0); }
        redraw(d);
        return;
    }
    if (action != RESTART_VM && action != SHUT_DOWN_VM) d->pending_power = NONE;
    enum page previous_page = d->page;
    switch (action) {
    case SHOW_WORKSPACE:
        if (d->settings_mode) { XCloseDisplay(d->display); exit(0); }
        d->page = WORKSPACE;
        break;
    case SHOW_MENU:
        if (d->settings_mode) d->page = MENU;
        else open_panel(d, "_HEURISM_LAUNCHER", "--launcher");
        break;
    case SHOW_SPACES: open_panel(d, "_HEURISM_SPACES", "--spaces"); break;
    case SHOW_QUICK: open_panel(d, "_HEURISM_QUICK_PANEL", "--quick"); break;
    case SHOW_OVERVIEW: d->page = OVERVIEW; break;
    case SHOW_SETTINGS: d->page = SETTINGS; break;
    case SHOW_DEVICE: d->page = DEVICE; break;
    case SHOW_NETWORK: d->page = NETWORK; break;
    case SHOW_SOUND: if (d->dell) d->page = SOUND; break;
    case SHOW_POWER: launch_self(d, "--power"); break;
    case LOCK_DESKTOP: launch(d, "/usr/bin/xfce4-screensaver-command", "--lock"); break;
    case LAUNCH_FILES: launch_native(d, "heurism-files"); break;
    case LAUNCH_EDITOR: launch_native(d, "heurism-editor"); break;
    case LAUNCH_BROWSER: launch(d, "/usr/bin/firefox", NULL); break;
    case LAUNCH_TERMINAL: launch_native(d, "heurism-terminal"); break;
    case LAUNCH_KEYBOARD: launch(d, "/usr/bin/onboard", NULL); break;
    case TOGGLE_THEME: {
        struct json_object *data = NULL;
        if (send_request(d, "theme", d->light ? "\"night\"" : "\"light\"", &data)) {
            d->light = !d->light;
            d->notice[0] = 0;
        }
        if (data) json_object_put(data);
        break;
    }
    case SWITCH_TASK: activate_task(d, index); break;
    case SWITCH_WORKSPACE: switch_workspace(d, index); break;
    case MOVE_WINDOW_WORKSPACE: move_active_window(d, index); break;
    case MINIMIZE_TASKS:
        for (int i = 0; i < d->task_count; i++) XIconifyWindow(d->display, d->tasks[i].window, d->screen);
        d->page = WORKSPACE;
        break;
    case ADMIN_CONSOLE:
        snprintf(d->notice, sizeof d->notice, "Use authenticated root SSH for administration");
        break;
    case BRIGHTER:
    case DIMMER: {
        int target = d->brightness + (action == BRIGHTER ? 10 : -10);
        if (target < 5) target = 5;
        if (target > 100) target = 100;
        char value[16];
        snprintf(value, sizeof value, "%d", target);
        struct json_object *data = NULL;
        if (send_request(d, "brightness", value, &data)) refresh_status(d);
        if (data) json_object_put(data);
        break;
    }
    case TAP_TOGGLE:
    case SCROLL_TOGGLE:
    case SPEED_UP:
    case SPEED_DOWN: {
        char value[96];
        if (action == TAP_TOGGLE)
            snprintf(value, sizeof value, "{\"tap\":%s}", d->tap ? "false" : "true");
        else if (action == SCROLL_TOGGLE)
            snprintf(value, sizeof value, "{\"natural_scroll\":%s}",
                     d->natural_scroll ? "false" : "true");
        else {
            double speed = d->pointer_speed + (action == SPEED_UP ? 0.1 : -0.1);
            if (speed > 1) speed = 1;
            if (speed < -1) speed = -1;
            snprintf(value, sizeof value, "{\"speed\":%.1f}", speed);
        }
        struct json_object *data = NULL;
        if (send_request(d, "input-settings", value, &data)) refresh_page(d);
        if (data) json_object_put(data);
        break;
    }
    case WIFI_SCAN: {
        struct json_object *data = NULL;
        d->network_count = 0;
        snprintf(d->notice, sizeof d->notice, "Scanning Wi-Fi networks...");
        redraw(d);
        if (send_request(d, "network-scan", NULL, &data) && data) {
            struct json_object *networks = field(data, "networks");
            if (networks && json_object_get_type(networks) == json_type_array)
                for (size_t i = 0; i < json_object_array_length(networks) && i < 12; i++) {
                    struct json_object *name = json_object_array_get_idx(networks, i);
                    if (name && json_object_get_type(name) == json_type_string)
                        snprintf(d->networks[d->network_count++], sizeof d->networks[0],
                                 "%s", json_object_get_string(name));
                }
            snprintf(d->notice, sizeof d->notice, "%d networks found", d->network_count);
        }
        if (data) json_object_put(data);
        break;
    }
    case WIFI_SELECT:
        if (index >= 0 && index < d->network_count) {
            snprintf(d->wifi_ssid, sizeof d->wifi_ssid, "%s", d->networks[index]);
            explicit_bzero(d->wifi_password, sizeof d->wifi_password);
            d->password_focus = true;
            d->notice[0] = 0;
        }
        break;
    case WIFI_PASSWORD:
        d->password_focus = true;
        XSetInputFocus(d->display, d->background, RevertToPointerRoot, CurrentTime);
        break;
    case WIFI_CONNECT: {
        if (!d->wifi_ssid[0] || strlen(d->wifi_password) < 8) {
            snprintf(d->notice, sizeof d->notice, "Select a network and enter its WPA password");
            break;
        }
        struct json_object *values = json_object_new_object(), *data = NULL;
        json_object_object_add(values, "ssid", json_object_new_string(d->wifi_ssid));
        json_object_object_add(values, "password", json_object_new_string(d->wifi_password));
        snprintf(d->notice, sizeof d->notice, "Connecting to Wi-Fi; waiting for an address...");
        redraw(d);
        bool okay = send_request(d, "network-connect",
            json_object_to_json_string_ext(values, JSON_C_TO_STRING_PLAIN), &data);
        json_object_put(values);
        explicit_bzero(d->wifi_password, sizeof d->wifi_password);
        d->password_focus = false;
        if (okay) {
            snprintf(d->notice, sizeof d->notice, "Wi-Fi connected with an address");
            refresh_page(d);
        }
        if (data) json_object_put(data);
        break;
    }
    case SOUND_LEFT_UP:
    case SOUND_LEFT_DOWN:
    case SOUND_RIGHT_UP:
    case SOUND_RIGHT_DOWN:
    case SOUND_MUTE: {
        char value[64];
        if (action == SOUND_MUTE)
            snprintf(value, sizeof value, "{\"muted\":%s}", d->muted ? "false" : "true");
        else {
            bool left_channel = action == SOUND_LEFT_UP || action == SOUND_LEFT_DOWN;
            int target = (left_channel ? d->sound_left : d->sound_right) +
                         ((action == SOUND_LEFT_UP || action == SOUND_RIGHT_UP) ? 10 : -10);
            if (target < 0) target = 0;
            if (target > 100) target = 100;
            snprintf(value, sizeof value, "{\"volume_left\":%d,\"volume_right\":%d}",
                     left_channel ? target : d->sound_left,
                     left_channel ? d->sound_right : target);
        }
        struct json_object *data = NULL;
        if (send_request(d, "sound-settings", value, &data)) refresh_page(d);
        if (data) json_object_put(data);
        break;
    }
    case BIOS_SELECT:
        if (index >= 0 && index < 5) { d->bios_selected = index; d->bios_choice = -1; }
        break;
    case BIOS_NEXT: {
        int count = bios_value_count(d);
        if (count) d->bios_choice = (d->bios_choice + 1) % count;
        break;
    }
    case BIOS_APPLY: {
        char choice[128];
        if (d->bios_choice < 0 || !bios_value_at(d, d->bios_choice, choice, sizeof choice)) {
            snprintf(d->notice, sizeof d->notice, "Select a BIOS value first"); break;
        }
        struct json_object *value = json_object_new_object(), *data = NULL;
        json_object_object_add(value, "name", json_object_new_string(bios_name(d->bios_selected)));
        json_object_object_add(value, "value", json_object_new_string(choice));
        bool okay = send_request(d, "bios-set",
            json_object_to_json_string_ext(value, JSON_C_TO_STRING_PLAIN), &data);
        json_object_put(value);
        if (okay) {
            snprintf(d->notice, sizeof d->notice, "BIOS setting applied; restart may be required");
            d->bios_choice = -1;
            refresh_page(d);
        }
        if (data) json_object_put(data);
        break;
    }
    case RESTART_VM:
    case SHUT_DOWN_VM:
        if (d->pending_power == action && time(NULL) <= d->power_deadline) {
            struct json_object *data = NULL;
            send_request(d, "power", action == RESTART_VM ?
                "{\"operation\":\"reboot\",\"confirm\":true}" :
                "{\"operation\":\"poweroff\",\"confirm\":true}", &data);
            if (data) json_object_put(data);
            d->pending_power = NONE;
        } else {
            d->pending_power = action;
            d->power_deadline = time(NULL) + 10;
        }
        break;
    default: break;
    }
    if (d->page != previous_page) {
        if (previous_page == NETWORK) explicit_bzero(d->wifi_password, sizeof d->wifi_password);
        d->password_focus = false;
        d->notice[0] = 0;
        refresh_page(d);
    }
    redraw(d);
}

static void set_window_type(struct desktop *d, Window window, Atom type) {
    XChangeProperty(d->display, window, d->type_atom, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&type, 1);
}

static void set_borderless(struct desktop *d, Window window) {
    Atom motif = XInternAtom(d->display, "_MOTIF_WM_HINTS", False);
    unsigned long hints[5] = {2, 0, 0, 0, 0};
    XChangeProperty(d->display, window, motif, motif, 32, PropModeReplace,
                    (unsigned char *)hints, 5);
}

static void install_launcher_shortcut(struct desktop *d) {
    KeyCode space = XKeysymToKeycode(d->display, XK_space);
    if (!space) return;
    unsigned numlock = 0;
    KeyCode number = XKeysymToKeycode(d->display, XK_Num_Lock);
    XModifierKeymap *modifiers = XGetModifierMapping(d->display);
    if (modifiers) {
        for (int modifier = 0; modifier < 8; modifier++)
            for (int slot = 0; slot < modifiers->max_keypermod; slot++)
                if (number && modifiers->modifiermap[modifier * modifiers->max_keypermod + slot]
                              == number) numlock |= 1u << modifier;
        XFreeModifiermap(modifiers);
    }
    Window root = RootWindow(d->display, d->screen);
    XSelectInput(d->display, root, KeyPressMask);
    XSync(d->display, False);
    previous_x_error = XSetErrorHandler(shortcut_error);
    shortcut_grab_failed = false;
    XGrabKey(d->display, space, Mod4Mask, root, False, GrabModeAsync, GrabModeAsync);
    XSync(d->display, False);
    bool base_failed = shortcut_grab_failed;
    if (!base_failed) {
        unsigned extras[] = {LockMask, numlock, LockMask | numlock};
        for (size_t i = 0; i < sizeof extras / sizeof extras[0]; i++) {
            if (!extras[i]) continue;
            bool duplicate = false;
            for (size_t j = 0; j < i; j++) if (extras[i] == extras[j]) duplicate = true;
            if (!duplicate)
                XGrabKey(d->display, space, Mod4Mask | extras[i], root, False,
                         GrabModeAsync, GrabModeAsync);
        }
        XSync(d->display, False);
    }
    KeyCode overview = XKeysymToKeycode(d->display, XK_o);
    if (overview) {
        unsigned variants[] = {0, LockMask, numlock, LockMask | numlock};
        for (size_t i = 0; i < sizeof variants / sizeof variants[0]; i++) {
            bool duplicate = false;
            for (size_t j = 0; j < i; j++)
                if (variants[i] == variants[j]) duplicate = true;
            if (!duplicate)
                XGrabKey(d->display, overview, Mod4Mask | variants[i], root, False,
                         GrabModeAsync, GrabModeAsync);
        }
    }
    KeySym spaces[] = {XK_1, XK_2, XK_3, XK_4};
    unsigned extras[] = {0, LockMask, numlock, LockMask | numlock};
    for (size_t i = 0; i < sizeof spaces / sizeof spaces[0]; i++) {
        KeyCode code = XKeysymToKeycode(d->display, spaces[i]);
        if (!code) continue;
        for (int shifted = 0; shifted < 2; shifted++)
            for (size_t extra = 0; extra < sizeof extras / sizeof extras[0]; extra++) {
                bool duplicate = false;
                for (size_t prior = 0; prior < extra; prior++)
                    if (extras[prior] == extras[extra]) duplicate = true;
                if (!duplicate)
                    XGrabKey(d->display, code,
                             Mod4Mask | (shifted ? ShiftMask : 0) | extras[extra],
                             root, False, GrabModeAsync, GrabModeAsync);
            }
    }
    XSync(d->display, False);
    XSetErrorHandler(previous_x_error);
    if (shortcut_grab_failed)
        fprintf(stderr, "Heurism shortcuts: some Super key grabs are unavailable\n");
}

static bool setup_x(struct desktop *d) {
    d->display = XOpenDisplay(NULL);
    if (!d->display) return false;
    d->screen = DefaultScreen(d->display);
    d->visual = DefaultVisual(d->display, d->screen);
    if (d->visual->class != TrueColor) return false;
    d->width = DisplayWidth(d->display, d->screen);
    d->height = DisplayHeight(d->display, d->screen);
    int screen_width = d->width, screen_height = d->height;
    d->screen_width = screen_width;
    d->screen_height = screen_height;
    if (d->settings_mode) {
        if (d->spaces_mode) {
            if (d->width > 1100) d->width = 1100;
            if (d->height > 700) d->height = 700;
            if (d->width > screen_width - 40) d->width = screen_width - 40;
            if (d->height > screen_height - 174) d->height = screen_height - 174;
        } else if (d->quick_mode) {
            if (d->width > 520) d->width = 520;
            if (d->height > 540) d->height = 540;
            if (d->width > screen_width - 40) d->width = screen_width - 40;
            if (d->height > screen_height - 40) d->height = screen_height - 40;
        } else if (d->launcher_mode) {
            if (d->width > 700) d->width = 700;
            if (d->height > 660) d->height = 660;
            if (d->width > screen_width - 40) d->width = screen_width - 40;
            if (d->height > screen_height - 40) d->height = screen_height - 40;
        } else if (d->power_mode) {
            if (d->width > 620) d->width = 620;
            if (d->height > 380) d->height = 380;
        } else {
            if (d->width > 1100) d->width = 1100;
            int maximum_height = screen_height >= 900 ? 900 : 680;
            if (maximum_height > screen_height - 120) maximum_height = screen_height - 120;
            if (d->height > maximum_height) d->height = maximum_height;
        }
    }
    d->dock_width = d->width - 32;
    if (d->dock_width > 720) d->dock_width = 720;
    d->dock_x = (d->width - d->dock_width) / 2;
    d->dock_y = d->height - 94;
    d->type_atom = XInternAtom(d->display, "_NET_WM_WINDOW_TYPE", False);
    d->desktop_atom = XInternAtom(d->display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    d->dock_atom = XInternAtom(d->display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    d->state_atom = XInternAtom(d->display, "_NET_WM_STATE", False);
    d->skip_taskbar_atom = XInternAtom(d->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    d->strut_atom = XInternAtom(d->display, "_NET_WM_STRUT", False);
    d->client_list_atom = XInternAtom(d->display, "_NET_CLIENT_LIST", False);
    d->active_atom = XInternAtom(d->display, "_NET_ACTIVE_WINDOW", False);
    d->current_workspace_atom = XInternAtom(d->display, "_NET_CURRENT_DESKTOP", False);
    d->workspace_count_atom = XInternAtom(d->display, "_NET_NUMBER_OF_DESKTOPS", False);
    d->window_workspace_atom = XInternAtom(d->display, "_NET_WM_DESKTOP", False);
    d->moveresize_atom = XInternAtom(d->display, "_NET_MOVERESIZE_WINDOW", False);
    d->supported_atom = XInternAtom(d->display, "_NET_SUPPORTED", False);
    d->workarea_atom = XInternAtom(d->display, "_NET_WORKAREA", False);
    d->frame_extents_atom = XInternAtom(d->display, "_NET_FRAME_EXTENTS", False);
    d->maximized_horz_atom = XInternAtom(d->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    d->maximized_vert_atom = XInternAtom(d->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Window root = RootWindow(d->display, d->screen);
    int window_x = d->settings_mode ? (screen_width - d->width) / 2 : 0;
    int window_y = d->settings_mode ? (screen_height - d->height) / 2 : 0;
    if (d->spaces_mode)
        window_y = 48 + (screen_height - 48 - 94 - d->height) / 2;
    if (d->quick_mode) {
        window_x = screen_width - d->width - 24;
        window_y = screen_height - d->height - 94;
        if (window_y < 20) window_y = 20;
    }
    d->background = XCreateSimpleWindow(d->display, root,
                                         window_x, window_y,
                                         (unsigned)d->width, (unsigned)d->height,
                                         0, 0, 0);
    if (d->settings_mode) {
        XSizeHints hints = {.flags = PPosition | PSize | PMinSize | PMaxSize,
                            .x = window_x, .y = window_y,
                            .width = d->width, .height = d->height,
                            .min_width = d->width, .min_height = d->height,
                            .max_width = d->width, .max_height = d->height};
        XSetWMNormalHints(d->display, d->background, &hints);
    }
    d->dock = XCreateSimpleWindow(d->display, root, d->dock_x, d->dock_y,
                                   (unsigned)d->dock_width, 78, 0, 0, 0);
    d->panel = XCreateSimpleWindow(d->display, root, 0, 0,
                                    (unsigned)d->width, 48, 0, 0, 0);
    XStoreName(d->display, d->background,
               d->spaces_mode ? "Heurism Spaces" :
               d->launcher_mode ? "Heurism Launcher" :
               d->quick_mode ? "Heurism Quick Controls" :
               d->power_mode ? "Heurism Power" :
               d->settings_mode && d->page == NETWORK ? "Heurism Network" :
               d->settings_mode && d->page == OVERVIEW ? "Heurism System" :
               d->settings_mode ? "Heurism Settings" : "Heurism desktop");
    XStoreName(d->display, d->dock, "Heurism dock");
    XStoreName(d->display, d->panel, "Heurism panel");
    if (!d->settings_mode) {
        set_window_type(d, d->background, d->desktop_atom);
        set_window_type(d, d->dock, d->dock_atom);
        set_window_type(d, d->panel, d->dock_atom);
    } else if (d->launcher_mode || d->quick_mode || d->spaces_mode) {
        XChangeProperty(d->display, d->background, d->state_atom, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&d->skip_taskbar_atom, 1);
        set_borderless(d, d->background);
    }
    long strut[4] = {0, 0, 0, 94};
    if (!d->settings_mode) {
        XChangeProperty(d->display, d->dock, d->strut_atom, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)strut, 4);
        strut[2] = 48;
        strut[3] = 0;
        XChangeProperty(d->display, d->panel, d->strut_atom, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)strut, 4);
    }
    XSelectInput(d->display, d->background, ExposureMask | ButtonPressMask | KeyPressMask);
    XSelectInput(d->display, d->dock, ExposureMask | ButtonPressMask | KeyPressMask);
    XSelectInput(d->display, d->panel, ExposureMask | ButtonPressMask | KeyPressMask);
    d->gc = XCreateGC(d->display, d->background, 0, NULL);
    Colormap colormap = DefaultColormap(d->display, d->screen);
    d->background_draw = XftDrawCreate(d->display, d->background, d->visual, colormap);
    d->dock_draw = XftDrawCreate(d->display, d->dock, d->visual, colormap);
    d->panel_draw = XftDrawCreate(d->display, d->panel, d->visual, colormap);
    d->font_small = XftFontOpenName(d->display, d->screen, "DejaVu Sans:size=12");
    d->font_body = XftFontOpenName(d->display, d->screen, "DejaVu Sans:size=15");
    d->font_large = XftFontOpenName(d->display, d->screen, "DejaVu Sans:bold:size=34");
    if (!d->background_draw || !d->dock_draw || !d->panel_draw ||
        !d->font_small || !d->font_body ||
        !d->font_large) return false;
    XMapWindow(d->display, d->background);
    if (!d->settings_mode) {
        XMapRaised(d->display, d->dock);
        XMapRaised(d->display, d->panel);
    }
    XSync(d->display, False);
    if (d->launcher_mode || d->quick_mode || d->spaces_mode) {
        Atom selection = XInternAtom(d->display,
            d->launcher_mode ? "_HEURISM_LAUNCHER" :
            d->spaces_mode ? "_HEURISM_SPACES" : "_HEURISM_QUICK_PANEL", False);
        XSetSelectionOwner(d->display, selection, d->background, CurrentTime);
        activate_window(d, d->background);
        XFlush(d->display);
    }
    if (!d->settings_mode && !strcmp(DisplayString(d->display), ":0")) {
        XWarpPointer(d->display, None, root, 0, 0, 0, 0, 48, 48);
        XSync(d->display, False);
    }
    return true;
}

static int show_home(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) return 1;
    int screen = DefaultScreen(display);
    Atom list = XInternAtom(display, "_NET_CLIENT_LIST", False);
    Atom type_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom desktop_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Atom dock_atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    Atom actual;
    int format;
    unsigned long count, remaining;
    unsigned char *clients = NULL;
    if (XGetWindowProperty(display, RootWindow(display, screen), list, 0, 256, False,
                           XA_WINDOW, &actual, &format, &count, &remaining, &clients) == Success &&
        actual == XA_WINDOW && format == 32) {
        for (unsigned long i = 0; i < count; i++) {
            Window window = ((Window *)clients)[i];
            unsigned char *types = NULL;
            Atom found;
            int bits;
            unsigned long number, rest;
            bool shell = false;
            if (XGetWindowProperty(display, window, type_atom, 0, 16, False, XA_ATOM,
                                   &found, &bits, &number, &rest, &types) == Success &&
                found == XA_ATOM && bits == 32) {
                for (unsigned long j = 0; j < number; j++)
                    if (((Atom *)types)[j] == desktop_atom || ((Atom *)types)[j] == dock_atom)
                        shell = true;
            }
            if (types) XFree(types);
            if (!shell) XIconifyWindow(display, window, screen);
        }
    }
    if (clients) XFree(clients);
    XCloseDisplay(display);
    return 0;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism desktop 0.8 (C/X11/Xft)"); return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--home")) return show_home();
    struct desktop d = {.page = WORKSPACE, .control_socket = DEFAULT_SOCKET,
                        .published_page = -1, .bios_choice = -1};
    if (argc == 2 && !strcmp(argv[1], "--settings")) {
        d.settings_mode = true;
        d.page = SETTINGS;
    } else if (argc == 2 && !strcmp(argv[1], "--launcher")) {
        d.settings_mode = true;
        d.launcher_mode = true;
        d.page = MENU;
    } else if (argc == 2 && !strcmp(argv[1], "--spaces")) {
        d.settings_mode = true;
        d.spaces_mode = true;
    } else if (argc == 2 && !strcmp(argv[1], "--quick")) {
        d.settings_mode = true;
        d.quick_mode = true;
    } else if (argc == 2 && !strcmp(argv[1], "--overview")) {
        d.settings_mode = true;
        d.page = OVERVIEW;
    } else if (argc == 2 && !strcmp(argv[1], "--network")) {
        d.settings_mode = true;
        d.page = NETWORK;
    } else if (argc == 2 && !strcmp(argv[1], "--power")) {
        d.settings_mode = true;
        d.power_mode = true;
        d.page = MENU;
    } else if (argc == 3 && !strcmp(argv[1], "--socket")) d.control_socket = argv[2];
    else if (argc != 1) return fprintf(stderr, "usage: heurism-desktop [--settings|--launcher|--spaces|--quick|--overview|--network|--power|--socket path]\n"), 2;
    signal(SIGCHLD, SIG_IGN);
    if (!setup_x(&d)) return fprintf(stderr, "heurism-desktop: X display unavailable\n"), 1;
    if (d.launcher_mode) {
        load_installed_apps(&d);
        load_local_files(&d);
    }
    if (!d.settings_mode) install_launcher_shortcut(&d);
    refresh_status(&d);
    refresh_page(&d);
    refresh_workspaces(&d);
    d.spaces_selected = d.current_workspace;
    refresh_tasks(&d);
    if (!d.settings_mode) ensure_visible(&d);
    redraw(&d);
    XSync(d.display, False);
    if (!d.settings_mode) publish_health(&d);
    time_t last_network_refresh = 0;
    for (;;) {
        while (XPending(d.display)) {
            XEvent event;
            XNextEvent(d.display, &event);
            if (event.type == Expose && !event.xexpose.count) redraw(&d);
            else if (event.type == ButtonPress) {
                for (int i = d.hit_count - 1; i >= 0; i--) {
                    struct hit *item = &d.hits[i];
                    if (item->window == event.xbutton.window && event.xbutton.x >= item->x &&
                        event.xbutton.y >= item->y && event.xbutton.x < item->x + item->width &&
                        event.xbutton.y < item->y + item->height) {
                        run_action(&d, item->action, item->index); break;
                    }
                }
            } else if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (!d.settings_mode &&
                    event.xkey.window == RootWindow(d.display, d.screen) &&
                    (event.xkey.state & Mod4Mask)) {
                    if (key == XK_space) {
                        run_action(&d, SHOW_MENU, 0);
                        continue;
                    }
                    if (key == XK_o) {
                        run_action(&d, SHOW_SPACES, 0);
                        continue;
                    }
                    if (key >= XK_1 && key <= XK_4) {
                        run_action(&d, event.xkey.state & ShiftMask ?
                                   MOVE_WINDOW_WORKSPACE : SWITCH_WORKSPACE,
                                   (int)(key - XK_1));
                        continue;
                    }
                }
                if (d.spaces_mode) {
                    if (key == XK_Escape) run_action(&d, SHOW_WORKSPACE, 0);
                    else if (key >= XK_1 && key <= XK_4 &&
                             (unsigned)(key - XK_1) < d.workspace_count)
                        run_action(&d, SWITCH_WORKSPACE, (int)(key - XK_1));
                    else if (key == XK_Left || key == XK_Up) {
                        if (d.spaces_selected > 0) d.spaces_selected--;
                        redraw(&d);
                    } else if (key == XK_Right || key == XK_Down || key == XK_Tab) {
                        if (d.spaces_selected + 1 < d.workspace_count &&
                            d.spaces_selected + 1 < 4) d.spaces_selected++;
                        else d.spaces_selected = 0;
                        redraw(&d);
                    } else if (key == XK_Return || key == XK_KP_Enter)
                        run_action(&d, SWITCH_WORKSPACE, (int)d.spaces_selected);
                    continue;
                }
                if (d.launcher_mode) {
                    int count = launcher_count(&d);
                    if (key == XK_Escape) run_action(&d, SHOW_WORKSPACE, 0);
                    else if (key == XK_Down && d.launcher_selected + 1 < count)
                        d.launcher_selected++;
                    else if (key == XK_Up && d.launcher_selected > 0)
                        d.launcher_selected--;
                    else if ((key == XK_Return || key == XK_KP_Enter) && count) {
                        struct launcher_result choice = launcher_choice(&d, d.launcher_selected);
                        run_action(&d, choice.action, choice.index);
                    }
                    else if (key == XK_BackSpace) {
                        size_t length = strlen(d.launcher_query);
                        if (length) d.launcher_query[length - 1] = 0;
                        d.launcher_selected = d.launcher_offset = 0;
                    } else if (key == XK_Delete) {
                        d.launcher_query[0] = 0;
                        d.launcher_selected = d.launcher_offset = 0;
                    } else {
                        char typed[16];
                        KeySym converted;
                        int length = XLookupString(&event.xkey, typed, sizeof typed, &converted, NULL);
                        size_t used = strlen(d.launcher_query);
                        if (length == 1 && typed[0] >= 32 && typed[0] < 127 &&
                            used + 1 < sizeof d.launcher_query) {
                            d.launcher_query[used] = typed[0];
                            d.launcher_query[used + 1] = 0;
                            d.launcher_selected = d.launcher_offset = 0;
                        }
                    }
                    redraw(&d);
                    continue;
                }
                if (d.quick_mode) {
                    if (key == XK_Escape) run_action(&d, SHOW_WORKSPACE, 0);
                    continue;
                }
                if (d.page == NETWORK && d.password_focus) {
                    size_t length = strlen(d.wifi_password);
                    if (key == XK_Return || key == XK_KP_Enter) run_action(&d, WIFI_CONNECT, 0);
                    else if (key == XK_Escape) { d.password_focus = false; redraw(&d); }
                    else if (key == XK_BackSpace && length) {
                        do { length--; } while (length &&
                            ((unsigned char)d.wifi_password[length] & 0xc0) == 0x80);
                        d.wifi_password[length] = 0;
                        redraw(&d);
                    } else {
                        char typed[32];
                        KeySym converted;
                        int count = XLookupString(&event.xkey, typed, sizeof typed, &converted, NULL);
                        bool valid = count > 0 && length + (size_t)count < sizeof d.wifi_password;
                        for (int i = 0; valid && i < count; i++)
                            if ((unsigned char)typed[i] < 32 || (unsigned char)typed[i] == 127)
                                valid = false;
                        if (valid) {
                            memcpy(d.wifi_password + length, typed, (size_t)count);
                            d.wifi_password[length + (size_t)count] = 0;
                            redraw(&d);
                        }
                    }
                    continue;
                }
                enum action action = key == XK_F1 ? SHOW_OVERVIEW :
                                     key == XK_F2 ? SHOW_SETTINGS :
                                     key == XK_F3 ? SHOW_DEVICE :
                                     key == XK_F5 ? SHOW_NETWORK :
                                     key == XK_F6 ? SHOW_SOUND :
                                     key == XK_F4 || key == XK_Escape ?
                                         d.power_mode ? SHOW_WORKSPACE :
                                         (d.settings_mode ? SHOW_MENU : SHOW_WORKSPACE) : NONE;
                if (action != NONE) run_action(&d, action, 0);
            }
        }
        struct pollfd input = {.fd = ConnectionNumber(d.display), .events = POLLIN};
        int ready = poll(&input, 1, 1000);
        if (ready < 0 && errno != EINTR) break;
        refresh_status(&d);
        time_t now = time(NULL);
        if (d.page == NETWORK && now - last_network_refresh >= 3) {
            refresh_page(&d);
            last_network_refresh = now;
        }
        refresh_tasks(&d);
        refresh_workspaces(&d);
        if (!d.settings_mode) ensure_visible(&d);
        redraw(&d);
        XSync(d.display, False);
        if (!d.settings_mode) publish_health(&d);
    }
    XCloseDisplay(d.display);
    return 1;
}

#define _GNU_SOURCE
/* Heurism Files and Editor share one small X11/Xft executable. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILE (4 * 1024 * 1024)
#define MAX_ENTRIES 2000
#define ROW_HEIGHT 32

enum mode { FILES, EDITOR };
enum prompt { NO_PROMPT, NEW_FOLDER, RENAME, TRASH_CONFIRM, OPEN_PATH, SAVE_PATH };
enum command { HOME, UP, OPEN, CREATE, CHANGE_NAME, TRASH, RESTORE, REFRESH,
               TRASH_BIN, NEW, SAVE, SAVE_AS };
struct entry { char name[NAME_MAX + 1]; bool directory; off_t size; };
struct button { int x, width; enum command command; const char *label; };
struct app {
    Display *display;
    Window window;
    Atom delete_window;
    Visual *visual;
    GC gc;
    XftDraw *draw;
    XftFont *font, *font_small;
    XIM input_method;
    XIC input_context;
    int screen, width, height;
    enum mode mode;
    enum prompt prompt;
    char home[PATH_MAX], directory[PATH_MAX], file[PATH_MAX], draft[PATH_MAX];
    char prompt_text[PATH_MAX], notice[256];
    struct entry entries[MAX_ENTRIES];
    int entry_count, selected, scroll;
    struct button buttons[12];
    int button_count;
    char *text;
    size_t length, cursor;
    int scroll_line;
    bool dirty;
    time_t last_draft;
    Time last_click;
};

static unsigned long pixel(struct app *a, unsigned red, unsigned green, unsigned blue) {
    unsigned long parts[] = {red, green, blue};
    unsigned long masks[] = {a->visual->red_mask, a->visual->green_mask, a->visual->blue_mask};
    unsigned long result = 0;
    for (int i = 0; i < 3; i++) {
        unsigned long mask = masks[i];
        unsigned shift = 0;
        if (!mask) continue;
        while (!(mask & 1)) { mask >>= 1; shift++; }
        result |= ((parts[i] * mask + 127) / 255) << shift;
    }
    return result;
}

static void box(struct app *a, int x, int y, int width, int height,
                unsigned red, unsigned green, unsigned blue) {
    if (width <= 0 || height <= 0) return;
    XSetForeground(a->display, a->gc, pixel(a, red, green, blue));
    XFillRectangle(a->display, a->window, a->gc, x, y, (unsigned)width, (unsigned)height);
}

static void text(struct app *a, int x, int y, const char *value, XftFont *font,
                 unsigned red, unsigned green, unsigned blue) {
    XftColor color = {.pixel = pixel(a, red, green, blue),
                      .color = {(unsigned short)(red * 257), (unsigned short)(green * 257),
                                (unsigned short)(blue * 257), 65535}};
    XftDrawStringUtf8(a->draw, &color, font, x, y, (const FcChar8 *)value, (int)strlen(value));
}

static bool path_join(char *out, size_t size, const char *parent, const char *name) {
    int n = snprintf(out, size, "%s/%s", parent, name);
    return n > 0 && n < (int)size;
}

static bool valid_basename(const char *name) {
    return *name && strcmp(name, ".") && strcmp(name, "..") &&
           !strchr(name, '/') && strlen(name) <= NAME_MAX;
}

static bool ensure_directory(const char *path, mode_t mode) {
    if (!mkdir(path, mode)) return true;
    struct stat info;
    return errno == EEXIST && !lstat(path, &info) && S_ISDIR(info.st_mode);
}

static bool write_all(int fd, const char *data, size_t length) {
    while (length) {
        ssize_t n = write(fd, data, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n; length -= (size_t)n;
    }
    return true;
}

static bool atomic_file(const char *destination, const char *data, size_t length) {
    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof temporary, "%s.new.XXXXXX", destination) >= (int)sizeof temporary)
        return false;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    struct stat old;
    mode_t mode = !stat(destination, &old) ? old.st_mode & 0777 : 0600;
    bool good = fchmod(fd, mode) == 0 && write_all(fd, data, length) && fsync(fd) == 0;
    if (close(fd)) good = false;
    if (good) good = rename(temporary, destination) == 0;
    if (!good) unlink(temporary);
    return good;
}

static int compare_entries(const void *left, const void *right) {
    const struct entry *a = left, *b = right;
    if (a->directory != b->directory) return a->directory ? -1 : 1;
    return strcasecmp(a->name, b->name);
}

static bool is_trash(struct app *a) {
    char path[PATH_MAX];
    return path_join(path, sizeof path, a->home, ".local/share/heurism/trash") &&
           !strcmp(a->directory, path);
}

static void list_directory(struct app *a) {
    DIR *dir = opendir(a->directory);
    if (!dir) { snprintf(a->notice, sizeof a->notice, "Cannot open folder: %s", strerror(errno)); return; }
    a->entry_count = 0;
    struct dirent *item;
    while ((item = readdir(dir))) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (is_trash(a) && strstr(item->d_name, ".origin.json") &&
            strlen(item->d_name) >= strlen(".origin.json") &&
            !strcmp(item->d_name + strlen(item->d_name) - strlen(".origin.json"), ".origin.json"))
            continue;
        if (a->entry_count >= MAX_ENTRIES) {
            snprintf(a->notice, sizeof a->notice, "More than %d entries; use Terminal", MAX_ENTRIES);
            break;
        }
        struct entry *entry = &a->entries[a->entry_count];
        snprintf(entry->name, sizeof entry->name, "%s", item->d_name);
        char path[PATH_MAX];
        struct stat info;
        if (!path_join(path, sizeof path, a->directory, entry->name) || stat(path, &info)) continue;
        entry->directory = S_ISDIR(info.st_mode);
        entry->size = info.st_size;
        a->entry_count++;
    }
    closedir(dir);
    qsort(a->entries, (size_t)a->entry_count, sizeof a->entries[0], compare_entries);
    a->selected = -1;
    a->scroll = 0;
}

static void navigate(struct app *a, const char *path) {
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) {
        snprintf(a->notice, sizeof a->notice, "Folder unavailable: %s", strerror(errno)); return;
    }
    struct stat info;
    if (stat(resolved, &info) || !S_ISDIR(info.st_mode)) {
        snprintf(a->notice, sizeof a->notice, "Select a folder"); return;
    }
    snprintf(a->directory, sizeof a->directory, "%s", resolved);
    a->notice[0] = 0;
    list_directory(a);
}

static bool selected_path(struct app *a, char *path, size_t size) {
    return a->selected >= 0 && a->selected < a->entry_count &&
           path_join(path, size, a->directory, a->entries[a->selected].name);
}

static void open_selected(struct app *a) {
    char path[PATH_MAX];
    if (!selected_path(a, path, sizeof path)) return;
    if (a->entries[a->selected].directory) { navigate(a, path); return; }
    pid_t pid = fork();
    if (pid < 0) { snprintf(a->notice, sizeof a->notice, "Could not open file"); return; }
    if (!pid) {
        char program[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", program, sizeof program - 1);
        if (length <= 0 || length >= (ssize_t)sizeof program - 1) _exit(127);
        program[length] = 0;
        char *slash = strrchr(program, '/');
        if (!slash || (size_t)(slash - program) + strlen("/heurism-editor") >= sizeof program)
            _exit(127);
        strcpy(slash + 1, "heurism-editor");
        execl(program, program, path, (char *)NULL);
        _exit(127);
    }
}

static bool prompt_basename(struct app *a, char *target, size_t size) {
    if (!valid_basename(a->prompt_text) || !path_join(target, size, a->directory, a->prompt_text)) {
        snprintf(a->notice, sizeof a->notice, "Use one nonempty filename"); return false;
    }
    return true;
}

static void trash_selected(struct app *a) {
    char source[PATH_MAX], trash[PATH_MAX], destination[PATH_MAX], metadata[PATH_MAX];
    if (!selected_path(a, source, sizeof source) || is_trash(a) ||
        !path_join(trash, sizeof trash, a->home, ".local/share/heurism/trash")) return;
    char share[PATH_MAX], heurism[PATH_MAX];
    if (!path_join(share, sizeof share, a->home, ".local/share") ||
        !path_join(heurism, sizeof heurism, share, "heurism") ||
        !ensure_directory(share, 0700) || !ensure_directory(heurism, 0700) ||
        !ensure_directory(trash, 0700)) goto failed;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    char name[NAME_MAX + 48];
    snprintf(name, sizeof name, "%lld-%ld-%s", (long long)now.tv_sec,
             now.tv_nsec, a->entries[a->selected].name);
    if (!path_join(destination, sizeof destination, trash, name) ||
        snprintf(metadata, sizeof metadata, "%s.origin.json", destination) >= (int)sizeof metadata)
        goto failed;
    struct stat existing;
    if (!lstat(destination, &existing) || !lstat(metadata, &existing)) goto failed;
    struct json_object *record = json_object_new_object();
    json_object_object_add(record, "original", json_object_new_string(source));
    const char *json = json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN);
    bool saved = atomic_file(metadata, json, strlen(json));
    json_object_put(record);
    if (!saved) goto failed;
    if (rename(source, destination)) { unlink(metadata); goto failed; }
    snprintf(a->notice, sizeof a->notice, "Moved to Trash");
    list_directory(a);
    return;
failed:
    snprintf(a->notice, sizeof a->notice, "Could not move to Trash: %s", strerror(errno));
}

static void restore_selected(struct app *a) {
    char source[PATH_MAX], metadata[PATH_MAX];
    if (!is_trash(a) || !selected_path(a, source, sizeof source) ||
        snprintf(metadata, sizeof metadata, "%s.origin.json", source) >= (int)sizeof metadata)
        return;
    FILE *file = fopen(metadata, "r");
    if (!file) goto failed;
    char data[PATH_MAX + 128];
    size_t length = fread(data, 1, sizeof data - 1, file);
    fclose(file);
    data[length] = 0;
    struct json_object *record = json_tokener_parse(data), *path_value = NULL;
    if (!record || !json_object_object_get_ex(record, "original", &path_value) ||
        json_object_get_type(path_value) != json_type_string) {
        if (record) json_object_put(record);
        goto failed;
    }
    const char *destination = json_object_get_string(path_value);
    char parent[PATH_MAX];
    if (*destination != '/' || strlen(destination) >= sizeof parent) {
        json_object_put(record); goto failed;
    }
    strcpy(parent, destination);
    char *slash = strrchr(parent, '/');
    if (!slash) { json_object_put(record); goto failed; }
    if (slash == parent) parent[1] = 0;
    else *slash = 0;
    struct stat info;
    if (lstat(destination, &info) == 0 || stat(parent, &info) || !S_ISDIR(info.st_mode) ||
        rename(source, destination)) {
        json_object_put(record); goto failed;
    }
    json_object_put(record);
    unlink(metadata);
    snprintf(a->notice, sizeof a->notice, "Restored file");
    list_directory(a);
    return;
failed:
    snprintf(a->notice, sizeof a->notice, "Original location unavailable or occupied");
}

static bool set_file(struct app *a, const char *path) {
    struct stat info;
    if (stat(path, &info) || !S_ISREG(info.st_mode) || info.st_size > MAX_FILE || info.st_size < 0) {
        snprintf(a->notice, sizeof a->notice, "Text file unavailable or larger than 4 MiB"); return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    char *buffer = malloc((size_t)info.st_size + 1);
    if (!buffer) { fclose(file); return false; }
    size_t length = fread(buffer, 1, (size_t)info.st_size, file);
    bool good = !ferror(file) && length == (size_t)info.st_size && !memchr(buffer, 0, length);
    fclose(file);
    if (!good) { free(buffer); snprintf(a->notice, sizeof a->notice, "Not a UTF-8 text file"); return false; }
    buffer[length] = 0;
    free(a->text);
    a->text = buffer;
    a->length = length;
    a->cursor = 0;
    a->scroll_line = 0;
    a->dirty = false;
    snprintf(a->file, sizeof a->file, "%s", path);
    a->notice[0] = 0;
    return true;
}

static void save_draft(struct app *a) {
    if (!a->dirty || !a->text || !a->draft[0]) return;
    struct json_object *record = json_object_new_object();
    json_object_object_add(record, "path", a->file[0] ? json_object_new_string(a->file) : NULL);
    json_object_object_add(record, "text", json_object_new_string_len(a->text, (int)a->length));
    const char *data = json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN);
    if (!atomic_file(a->draft, data, strlen(data)))
        snprintf(a->notice, sizeof a->notice, "Draft could not be saved");
    json_object_put(record);
}

static void load_draft(struct app *a) {
    FILE *file = fopen(a->draft, "rb");
    if (!file) return;
    struct stat info;
    if (stat(a->draft, &info) || info.st_size < 0 || info.st_size > MAX_FILE * 2 + 128) {
        fclose(file); return;
    }
    char *data = malloc((size_t)info.st_size + 1);
    if (!data) { fclose(file); return; }
    size_t length = fread(data, 1, (size_t)info.st_size, file);
    fclose(file);
    data[length] = 0;
    struct json_object *record = json_tokener_parse(data);
    free(data);
    if (!record) return;
    struct json_object *content = NULL, *path = NULL;
    if (json_object_object_get_ex(record, "text", &content) &&
        json_object_get_type(content) == json_type_string &&
        json_object_get_string_len(content) <= MAX_FILE) {
        free(a->text);
        a->length = (size_t)json_object_get_string_len(content);
        a->text = strdup(json_object_get_string(content));
        a->cursor = a->length;
        a->dirty = true;
        if (json_object_object_get_ex(record, "path", &path) &&
            path && json_object_get_type(path) == json_type_string)
            snprintf(a->file, sizeof a->file, "%s", json_object_get_string(path));
        snprintf(a->notice, sizeof a->notice, "Recovered unsaved draft");
    }
    json_object_put(record);
}

static bool save_editor(struct app *a, const char *destination, bool new_path) {
    if (!*destination) { snprintf(a->notice, sizeof a->notice, "Enter a file path"); return false; }
    struct stat info;
    if (new_path && strcmp(destination, a->file) && lstat(destination, &info) == 0) {
        snprintf(a->notice, sizeof a->notice, "That file already exists"); return false;
    }
    if (!atomic_file(destination, a->text, a->length)) {
        snprintf(a->notice, sizeof a->notice, "Save failed: %s", strerror(errno)); return false;
    }
    if (destination != a->file) snprintf(a->file, sizeof a->file, "%s", destination);
    a->dirty = false;
    unlink(a->draft);
    snprintf(a->notice, sizeof a->notice, "Saved");
    return true;
}

static void prompt_start(struct app *a, enum prompt prompt, const char *initial) {
    a->prompt = prompt;
    snprintf(a->prompt_text, sizeof a->prompt_text, "%s", initial ? initial : "");
}

static bool editor_path(struct app *a, char *destination, size_t size) {
    const char *source = a->prompt_text;
    if (!*source) return false;
    if (source[0] == '~' && source[1] == '/') return path_join(destination, size, a->home, source + 2);
    if (source[0] == '/') {
        if (strlen(source) >= size) return false;
        strcpy(destination, source); return true;
    }
    return path_join(destination, size, a->home, source);
}

static void submit_prompt(struct app *a) {
    char target[PATH_MAX], source[PATH_MAX];
    switch (a->prompt) {
    case NEW_FOLDER:
        if (prompt_basename(a, target, sizeof target)) {
            if (mkdir(target, 0700)) snprintf(a->notice, sizeof a->notice, "Create failed: %s", strerror(errno));
            else { list_directory(a); snprintf(a->notice, sizeof a->notice, "Folder created"); }
        }
        break;
    case RENAME:
        if (prompt_basename(a, target, sizeof target) && selected_path(a, source, sizeof source)) {
            struct stat info;
            if (!strcmp(source, target)) snprintf(a->notice, sizeof a->notice, "Name unchanged");
            else if (!lstat(target, &info)) snprintf(a->notice, sizeof a->notice, "That name already exists");
            else if (rename(source, target)) snprintf(a->notice, sizeof a->notice, "Rename failed: %s", strerror(errno));
            else { list_directory(a); snprintf(a->notice, sizeof a->notice, "Renamed"); }
        }
        break;
    case TRASH_CONFIRM:
        if (!strcmp(a->prompt_text, "yes")) trash_selected(a);
        break;
    case OPEN_PATH:
        if (editor_path(a, target, sizeof target)) {
            if (a->dirty) snprintf(a->notice, sizeof a->notice, "Save this draft before opening another file");
            else set_file(a, target);
        }
        break;
    case SAVE_PATH:
        if (editor_path(a, target, sizeof target)) save_editor(a, target, true);
        break;
    default: break;
    }
    a->prompt = NO_PROMPT;
}

static void add_button(struct app *a, const char *name, enum command command, int width) {
    if (a->button_count >= 12) return;
    int x = a->button_count ? a->buttons[a->button_count - 1].x +
                              a->buttons[a->button_count - 1].width + 8 : 22;
    a->buttons[a->button_count++] = (struct button){x, width, command, name};
}

static void configure_buttons(struct app *a) {
    a->button_count = 0;
    if (a->mode == FILES) {
        add_button(a, "Home", HOME, 68);
        add_button(a, "Up", UP, 52);
        add_button(a, "Open", OPEN, 68);
        add_button(a, "New folder", CREATE, 110);
        add_button(a, "Rename", CHANGE_NAME, 86);
        add_button(a, "Trash", TRASH, 72);
        add_button(a, "Trash bin", TRASH_BIN, 94);
        add_button(a, "Restore", RESTORE, 84);
        add_button(a, "Refresh", REFRESH, 86);
    } else {
        add_button(a, "New", NEW, 70);
        add_button(a, "Open", OPEN, 74);
        add_button(a, "Save", SAVE, 74);
        add_button(a, "Save as", SAVE_AS, 95);
    }
}

static void command(struct app *a, enum command action) {
    char path[PATH_MAX];
    switch (action) {
    case HOME: navigate(a, a->home); break;
    case UP:
        snprintf(path, sizeof path, "%s", a->directory);
        char *slash = strrchr(path, '/');
        if (slash && slash != path) *slash = 0;
        else strcpy(path, "/");
        navigate(a, path); break;
    case OPEN:
        if (a->mode == FILES) open_selected(a);
        else prompt_start(a, OPEN_PATH, a->home);
        break;
    case CREATE: prompt_start(a, NEW_FOLDER, ""); break;
    case CHANGE_NAME:
        if (a->selected >= 0) prompt_start(a, RENAME, a->entries[a->selected].name);
        break;
    case TRASH:
        if (a->selected >= 0 && !is_trash(a)) prompt_start(a, TRASH_CONFIRM, "");
        break;
    case RESTORE: restore_selected(a); break;
    case REFRESH: list_directory(a); break;
    case TRASH_BIN:
        if (path_join(path, sizeof path, a->home, ".local/share/heurism/trash")) {
            char share[PATH_MAX], heurism[PATH_MAX];
            if (path_join(share, sizeof share, a->home, ".local/share") &&
                path_join(heurism, sizeof heurism, share, "heurism") &&
                ensure_directory(share, 0700) && ensure_directory(heurism, 0700) &&
                ensure_directory(path, 0700)) navigate(a, path);
        }
        break;
    case NEW:
        if (a->dirty) snprintf(a->notice, sizeof a->notice, "Save this draft before creating another document");
        else { a->length = a->cursor = 0; a->text[0] = 0; a->file[0] = 0; a->scroll_line = 0; }
        break;
    case SAVE:
        if (a->file[0]) save_editor(a, a->file, false);
        else prompt_start(a, SAVE_PATH, "Documents/Untitled.txt");
        break;
    case SAVE_AS: prompt_start(a, SAVE_PATH, a->file[0] ? a->file : "Documents/Untitled.txt"); break;
    }
}

static size_t previous_character(const char *buffer, size_t offset) {
    if (!offset) return 0;
    offset--;
    while (offset && ((unsigned char)buffer[offset] & 0xc0) == 0x80) offset--;
    return offset;
}

static size_t next_character(const char *buffer, size_t length, size_t offset) {
    if (offset >= length) return length;
    offset++;
    while (offset < length && ((unsigned char)buffer[offset] & 0xc0) == 0x80) offset++;
    return offset;
}

static void insert_text(struct app *a, const char *bytes, size_t count) {
    if (a->length + count > MAX_FILE) { snprintf(a->notice, sizeof a->notice, "Document size limit reached"); return; }
    char *larger = realloc(a->text, a->length + count + 1);
    if (!larger) { snprintf(a->notice, sizeof a->notice, "Out of memory"); return; }
    a->text = larger;
    memmove(a->text + a->cursor + count, a->text + a->cursor, a->length - a->cursor + 1);
    memcpy(a->text + a->cursor, bytes, count);
    a->length += count;
    a->cursor += count;
    a->dirty = true;
}

static void delete_range(struct app *a, size_t start, size_t end) {
    if (start >= end || end > a->length) return;
    memmove(a->text + start, a->text + end, a->length - end + 1);
    a->length -= end - start;
    a->cursor = start;
    a->dirty = true;
}

static int cursor_line(struct app *a) {
    int line = 0;
    for (size_t i = 0; i < a->cursor; i++) if (a->text[i] == '\n') line++;
    return line;
}

static void ensure_cursor_visible(struct app *a) {
    int line = cursor_line(a);
    int rows = (a->height - 190) / ROW_HEIGHT;
    if (line < a->scroll_line) a->scroll_line = line;
    if (line >= a->scroll_line + rows) a->scroll_line = line - rows + 1;
    if (a->scroll_line < 0) a->scroll_line = 0;
}

static void move_vertical(struct app *a, int delta) {
    size_t start = a->cursor;
    while (start && a->text[start - 1] != '\n') start--;
    size_t column = a->cursor - start;
    if (delta < 0) {
        if (!start) return;
        size_t previous_end = start - 1, previous_start = previous_end;
        while (previous_start && a->text[previous_start - 1] != '\n') previous_start--;
        size_t length = previous_end - previous_start;
        a->cursor = previous_start + (column < length ? column : length);
    } else {
        size_t end = a->cursor;
        while (end < a->length && a->text[end] != '\n') end++;
        if (end == a->length) return;
        size_t next_start = end + 1, next_end = next_start;
        while (next_end < a->length && a->text[next_end] != '\n') next_end++;
        size_t length = next_end - next_start;
        a->cursor = next_start + (column < length ? column : length);
    }
    ensure_cursor_visible(a);
}

static void render(struct app *a) {
    box(a, 0, 0, a->width, a->height, 10, 19, 29);
    box(a, 0, 0, a->width, 116, 19, 30, 43);
    text(a, 24, 39, a->mode == FILES ? "Files" : "Editor", a->font, 240, 246, 255);
    for (int i = 0; i < a->button_count; i++) {
        struct button *button = &a->buttons[i];
        box(a, button->x, 57, button->width, 42, 35, 55, 72);
        text(a, button->x + 10, 84, button->label, a->font_small, 237, 246, 255);
    }
    if (a->mode == FILES) {
        char title[PATH_MAX + 16];
        snprintf(title, sizeof title, "Location: %s", a->directory);
        text(a, 24, 143, title, a->font_small, 157, 176, 198);
        int visible = (a->height - 205) / ROW_HEIGHT;
        for (int row = 0; row < visible && row + a->scroll < a->entry_count; row++) {
            int index = row + a->scroll, y = 161 + row * ROW_HEIGHT;
            struct entry *entry = &a->entries[index];
            if (index == a->selected) box(a, 18, y, a->width - 36, ROW_HEIGHT, 31, 83, 91);
            char name[128], details[64];
            snprintf(name, sizeof name, "%.100s", entry->name);
            if (entry->directory) snprintf(details, sizeof details, "Folder");
            else snprintf(details, sizeof details, "%lld B", (long long)entry->size);
            text(a, 32, y + 23, name, a->font_small, 240, 246, 255);
            text(a, a->width - 150, y + 23, details, a->font_small, 157, 176, 198);
        }
    } else {
        char name[PATH_MAX + 32];
        snprintf(name, sizeof name, "%s%s", a->file[0] ? a->file : "Untitled",
                 a->dirty ? "  ·  Unsaved" : "");
        text(a, 24, 142, name, a->font_small, 157, 176, 198);
        size_t offset = 0;
        int line = 0;
        while (offset < a->length && line < a->scroll_line) {
            if (a->text[offset++] == '\n') line++;
        }
        int visible = (a->height - 190) / ROW_HEIGHT;
        for (int row = 0; row < visible && offset <= a->length; row++) {
            size_t start = offset;
            while (offset < a->length && a->text[offset] != '\n') offset++;
            size_t end = offset;
            char content[4096];
            size_t count = end - start;
            if (count >= sizeof content) count = sizeof content - 1;
            memcpy(content, a->text + start, count);
            content[count] = 0;
            text(a, 26, 177 + row * ROW_HEIGHT, content, a->font, 240, 246, 255);
            if (a->cursor >= start && a->cursor <= end) {
                size_t prefix = a->cursor - start;
                if (prefix > count) prefix = count;
                XGlyphInfo extent;
                XftTextExtentsUtf8(a->display, a->font, (FcChar8 *)content, (int)prefix, &extent);
                box(a, 26 + extent.xOff, 181 + row * ROW_HEIGHT, 2, 3, 80, 225, 190);
            }
            if (offset == a->length) break;
            offset++;
        }
    }
    box(a, 0, a->height - 43, a->width, 43, 19, 30, 43);
    if (a->prompt != NO_PROMPT) {
        const char *title = a->prompt == NEW_FOLDER ? "New folder" :
                            a->prompt == RENAME ? "New name" :
                            a->prompt == TRASH_CONFIRM ? "Type yes to move to Trash" :
                            a->prompt == OPEN_PATH ? "Open path" : "Save path";
        char line[PATH_MAX + 64];
        snprintf(line, sizeof line, "%s: %s_", title, a->prompt_text);
        text(a, 24, a->height - 15, line, a->font_small, 80, 225, 190);
    } else text(a, 24, a->height - 15, a->notice, a->font_small, 80, 225, 190);
    XFlush(a->display);
}

static void keypress(struct app *a, XKeyEvent *event) {
    KeySym symbol = NoSymbol;
    char input[128];
    int count;
    if (a->input_context) {
        Status status;
        count = Xutf8LookupString(a->input_context, event, input, sizeof input, &symbol, &status);
        if (status == XBufferOverflow) return;
    } else count = XLookupString(event, input, sizeof input, &symbol, NULL);
    if (symbol == XK_Escape && a->prompt != NO_PROMPT) { a->prompt = NO_PROMPT; return; }
    if (a->prompt != NO_PROMPT) {
        size_t length = strlen(a->prompt_text);
        if ((event->state & ControlMask) && (symbol == XK_a || symbol == XK_A))
            a->prompt_text[0] = 0;
        else if (symbol == XK_Return) submit_prompt(a);
        else if (symbol == XK_BackSpace && length) a->prompt_text[previous_character(a->prompt_text, length)] = 0;
        else if (count > 0 && (unsigned char)input[0] >= 32 && length + (size_t)count < sizeof a->prompt_text) {
            memcpy(a->prompt_text + length, input, (size_t)count);
            a->prompt_text[length + (size_t)count] = 0;
        }
        return;
    }
    if (a->mode == FILES) {
        if (symbol == XK_Return) open_selected(a);
        else if (symbol == XK_Up && a->selected > 0) a->selected--;
        else if (symbol == XK_Down && a->selected + 1 < a->entry_count) a->selected++;
        else if (symbol == XK_Delete) command(a, TRASH);
        else if (symbol == XK_F2) command(a, CHANGE_NAME);
        if (a->selected < a->scroll) a->scroll = a->selected;
        int visible = (a->height - 205) / ROW_HEIGHT;
        if (a->selected >= a->scroll + visible) a->scroll = a->selected - visible + 1;
        if (a->scroll < 0) a->scroll = 0;
        return;
    }
    if (event->state & ControlMask) {
        if (symbol == XK_s) command(a, SAVE);
        else if (symbol == XK_o) command(a, OPEN);
        else if (symbol == XK_n) command(a, NEW);
        return;
    }
    switch (symbol) {
    case XK_Left: a->cursor = previous_character(a->text, a->cursor); break;
    case XK_Right: a->cursor = next_character(a->text, a->length, a->cursor); break;
    case XK_Up: move_vertical(a, -1); break;
    case XK_Down: move_vertical(a, 1); break;
    case XK_Home:
        while (a->cursor && a->text[a->cursor - 1] != '\n') a->cursor--;
        break;
    case XK_End:
        while (a->cursor < a->length && a->text[a->cursor] != '\n') a->cursor++;
        break;
    case XK_BackSpace:
        if (a->cursor) delete_range(a, previous_character(a->text, a->cursor), a->cursor);
        break;
    case XK_Delete:
        if (a->cursor < a->length) delete_range(a, a->cursor,
                                                next_character(a->text, a->length, a->cursor));
        break;
    case XK_Return: insert_text(a, "\n", 1); break;
    case XK_Tab: insert_text(a, "    ", 4); break;
    default:
        if (count > 0 && (unsigned char)input[0] >= 32 && !memchr(input, 0, (size_t)count))
            insert_text(a, input, (size_t)count);
        break;
    }
    ensure_cursor_visible(a);
}

static void click(struct app *a, XButtonEvent *event) {
    if (event->button == 4 || event->button == 5) {
        if (a->mode == FILES) {
            a->scroll += event->button == 5 ? 3 : -3;
            if (a->scroll < 0) a->scroll = 0;
            if (a->scroll >= a->entry_count) a->scroll = a->entry_count ? a->entry_count - 1 : 0;
        } else {
            a->scroll_line += event->button == 5 ? 3 : -3;
            if (a->scroll_line < 0) a->scroll_line = 0;
        }
        return;
    }
    if (event->button != 1) return;
    if (event->y >= 57 && event->y < 100) {
        for (int i = 0; i < a->button_count; i++) {
            struct button *button = &a->buttons[i];
            if (event->x >= button->x && event->x < button->x + button->width) {
                command(a, button->command); return;
            }
        }
    }
    if (a->mode == FILES && event->y >= 161 && event->y < a->height - 43) {
        int index = a->scroll + (event->y - 161) / ROW_HEIGHT;
        if (index >= 0 && index < a->entry_count) {
            bool double_click = a->selected == index && event->time - a->last_click < 350;
            a->selected = index;
            a->last_click = event->time;
            if (double_click) open_selected(a);
        }
    }
}

static bool setup(struct app *a) {
    a->display = XOpenDisplay(NULL);
    if (!a->display) return false;
    a->screen = DefaultScreen(a->display);
    a->visual = DefaultVisual(a->display, a->screen);
    if (a->visual->class != TrueColor) return false;
    a->width = 900; a->height = 640;
    a->window = XCreateSimpleWindow(a->display, RootWindow(a->display, a->screen),
                                    180, 100, (unsigned)a->width, (unsigned)a->height,
                                    0, 0, 0);
    XStoreName(a->display, a->window, a->mode == FILES ? "Heurism Files" : "Heurism Editor");
    XSelectInput(a->display, a->window, ExposureMask | ButtonPressMask |
                 KeyPressMask | StructureNotifyMask | FocusChangeMask);
    a->delete_window = XInternAtom(a->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(a->display, a->window, &a->delete_window, 1);
    a->gc = XCreateGC(a->display, a->window, 0, NULL);
    a->draw = XftDrawCreate(a->display, a->window, a->visual,
                             DefaultColormap(a->display, a->screen));
    a->font = XftFontOpenName(a->display, a->screen, "DejaVu Sans Mono:size=14");
    a->font_small = XftFontOpenName(a->display, a->screen, "DejaVu Sans:size=11");
    if (!a->draw || !a->font || !a->font_small) return false;
    a->input_method = XOpenIM(a->display, NULL, NULL, NULL);
    if (a->input_method)
        a->input_context = XCreateIC(a->input_method, XNInputStyle,
                                      XIMPreeditNothing | XIMStatusNothing,
                                      XNClientWindow, a->window,
                                      XNFocusWindow, a->window, NULL);
    XMapWindow(a->display, a->window);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism Files/Editor 0.1 (C/X11/Xft)"); return 0;
    }
    setlocale(LC_CTYPE, "");
    struct app a = {.selected = -1};
    const char *program = strrchr(argv[0], '/');
    program = program ? program + 1 : argv[0];
    a.mode = strstr(program, "editor") ? EDITOR : FILES;
    if (argc > 2) return fprintf(stderr, "invalid arguments\n"), 2;
    const char *home = getenv("HOME");
    if (!home || *home != '/' || strlen(home) >= sizeof a.home) return fprintf(stderr, "HOME unavailable\n"), 1;
    snprintf(a.home, sizeof a.home, "%s", home);
    if (a.mode == FILES) {
        char documents[PATH_MAX];
        if (!path_join(documents, sizeof documents, a.home, "Documents")) return 1;
        navigate(&a, argc == 2 ? argv[1] : access(documents, F_OK) ? a.home : documents);
    } else {
        a.text = strdup("");
        if (!a.text) return 1;
        char local[PATH_MAX], state[PATH_MAX];
        if (path_join(local, sizeof local, a.home, ".local") &&
            path_join(state, sizeof state, local, "state") &&
            path_join(a.draft, sizeof a.draft, state, "heurism/native-editor-draft.json")) {
            char heurism[PATH_MAX];
            if (path_join(heurism, sizeof heurism, state, "heurism") &&
                ensure_directory(local, 0700) && ensure_directory(state, 0700))
                ensure_directory(heurism, 0700);
        }
        if (argc == 2) set_file(&a, argv[1]);
        else load_draft(&a);
    }
    configure_buttons(&a);
    if (!setup(&a)) return fprintf(stderr, "Heurism application requires X11 TrueColor\n"), 1;
    render(&a);
    bool running = true;
    while (running) {
        while (XPending(a.display)) {
            XEvent event;
            XNextEvent(a.display, &event);
            if (event.type == Expose && !event.xexpose.count) render(&a);
            else if (event.type == ConfigureNotify) {
                a.width = event.xconfigure.width;
                a.height = event.xconfigure.height;
                render(&a);
            } else if (event.type == KeyPress) { keypress(&a, &event.xkey); render(&a); }
            else if (event.type == ButtonPress) { click(&a, &event.xbutton); render(&a); }
            else if (event.type == FocusIn && a.input_context) XSetICFocus(a.input_context);
            else if (event.type == FocusOut && a.input_context) XUnsetICFocus(a.input_context);
            else if (event.type == ClientMessage &&
                     (Atom)event.xclient.data.l[0] == a.delete_window) running = false;
        }
        time_t now = time(NULL);
        if (a.mode == EDITOR && a.dirty && now - a.last_draft >= 2) {
            save_draft(&a);
            a.last_draft = now;
        }
        struct pollfd input = {.fd = ConnectionNumber(a.display), .events = POLLIN};
        int ready = poll(&input, 1, 250);
        if (ready < 0 && errno != EINTR) break;
    }
    if (a.mode == EDITOR && a.dirty) save_draft(&a);
    if (a.input_context) XDestroyIC(a.input_context);
    if (a.input_method) XCloseIM(a.input_method);
    XftDrawDestroy(a.draw);
    XftFontClose(a.display, a.font);
    XftFontClose(a.display, a.font_small);
    XFreeGC(a.display, a.gc);
    XDestroyWindow(a.display, a.window);
    XCloseDisplay(a.display);
    free(a.text);
    return 0;
}

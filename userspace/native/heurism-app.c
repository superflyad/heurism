#define _GNU_SOURCE
/* Heurism Files and Editor share one small X11/Xft executable. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <gio/gio.h>
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
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILE (4 * 1024 * 1024)
#define MAX_ENTRIES 2000
#define FILE_ROW_HEIGHT 42
#define FILE_ROW_TOP 198
#define EDITOR_ROW_HEIGHT 30
#define EDITOR_ROW_TOP 198
#define MAX_UNDO 128

enum mode { FILES, EDITOR };
enum prompt { NO_PROMPT, NEW_FOLDER, RENAME, TRASH_CONFIRM, OPEN_PATH, SAVE_PATH };
enum command { HOME, UP, OPEN, CREATE, CHANGE_NAME, TRASH, RESTORE, REFRESH,
               TRASH_BIN, DOCUMENTS, DOWNLOADS, PICTURES, TOGGLE_HIDDEN,
               LOCATION, NEW, SAVE, SAVE_AS };
struct entry { char name[NAME_MAX + 1]; bool directory; off_t size; };
struct button { int x, width; enum command command; const char *label; };
struct edit {
    size_t start, removed_len, inserted_len, cursor_before;
    char *removed, *inserted;
    time_t when;
};
struct app {
    Display *display;
    Window window;
    Atom delete_window;
    Atom clipboard, utf8, targets, incr, paste_property;
    Visual *visual;
    GC gc;
    XftDraw *draw;
    XftFont *font, *font_small, *font_title;
    XIM input_method;
    XIC input_context;
    int screen, width, height;
    enum mode mode;
    enum prompt prompt;
    char home[PATH_MAX], directory[PATH_MAX], file[PATH_MAX], draft[PATH_MAX];
    char draft_lock_path[PATH_MAX];
    int draft_lock;
    char prompt_text[PATH_MAX], notice[256];
    struct entry entries[MAX_ENTRIES];
    int entry_count, selected, scroll;
    struct button buttons[12];
    int button_count;
    char *text;
    size_t length, capacity, cursor, anchor;
    bool selecting, owns_primary, owns_clipboard;
    char *primary_text, *clipboard_text, *paste_text;
    size_t primary_length, clipboard_length, paste_length;
    Atom pending_paste, paste_target;
    bool paste_incr;
    size_t paste_start, paste_end;
    time_t paste_deadline;
    int scroll_line, scroll_column;
    struct edit undo[MAX_UNDO];
    int undo_count, undo_position;
    bool dirty;
    bool show_hidden;
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

static void round_box(struct app *a, int x, int y, int width, int height, int radius,
                      unsigned red, unsigned green, unsigned blue) {
    if (width <= radius * 2 || height <= radius * 2) return;
    box(a, x + radius, y, width - radius * 2, height, red, green, blue);
    box(a, x, y + radius, width, height - radius * 2, red, green, blue);
    XSetForeground(a->display, a->gc, pixel(a, red, green, blue));
    int corners[4][2] = {{x, y}, {x + width - radius * 2, y},
                         {x, y + height - radius * 2},
                         {x + width - radius * 2, y + height - radius * 2}};
    for (int i = 0; i < 4; i++)
        XFillArc(a->display, a->window, a->gc, corners[i][0], corners[i][1],
                 (unsigned)(radius * 2), (unsigned)(radius * 2), 0, 360 * 64);
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
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..") ||
            (!a->show_hidden && item->d_name[0] == '.')) continue;
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
    char *type = g_content_type_guess(path, NULL, 0, NULL);
    bool text_file = type && g_content_type_is_a(type, "text/plain");
    g_free(type);
    if (!text_file) {
        GFile *file = g_file_new_for_path(path);
        char *uri = g_file_get_uri(file);
        GError *error = NULL;
        bool started = uri && g_app_info_launch_default_for_uri(uri, NULL, &error);
        g_clear_error(&error);
        g_free(uri);
        g_object_unref(file);
        if (!started) snprintf(a->notice, sizeof a->notice,
                               "No application is registered for this file");
        else a->notice[0] = 0;
        return;
    }
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
    char local[PATH_MAX], share[PATH_MAX], heurism[PATH_MAX];
    if (!path_join(local, sizeof local, a->home, ".local") ||
        !path_join(share, sizeof share, local, "share") ||
        !path_join(heurism, sizeof heurism, share, "heurism") ||
        !ensure_directory(local, 0700) || !ensure_directory(share, 0700) ||
        !ensure_directory(heurism, 0700) ||
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

static void free_edit(struct edit *edit) {
    free(edit->removed);
    free(edit->inserted);
    memset(edit, 0, sizeof *edit);
}

static void clear_undo(struct app *a) {
    for (int i = 0; i < a->undo_count; i++) free_edit(&a->undo[i]);
    a->undo_count = a->undo_position = 0;
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
    bool good = !ferror(file) && length == (size_t)info.st_size &&
                !memchr(buffer, 0, length) && g_utf8_validate(buffer, (gssize)length, NULL);
    fclose(file);
    if (!good) { free(buffer); snprintf(a->notice, sizeof a->notice, "Not a UTF-8 text file"); return false; }
    buffer[length] = 0;
    free(a->text);
    a->text = buffer;
    a->length = length;
    a->capacity = length + 1;
    a->cursor = 0;
    a->anchor = 0;
    a->scroll_line = a->scroll_column = 0;
    clear_undo(a);
    a->dirty = false;
    snprintf(a->file, sizeof a->file, "%s", path);
    a->notice[0] = 0;
    return true;
}

static int open_draft_lock(const char *draft, bool fresh, char *lock_path,
                           size_t lock_size) {
    if (snprintf(lock_path, lock_size, "%s.lock", draft) >= (int)lock_size) return -1;
    int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW;
    if (fresh) flags |= O_EXCL;
    int fd = open(lock_path, flags, 0600);
    if (fd < 0) return -1;
    struct stat info;
    if (fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_uid != getuid() ||
        (info.st_mode & 077) || flock(fd, LOCK_EX | LOCK_NB)) {
        close(fd);
        if (fresh) unlink(lock_path);
        return -1;
    }
    return fd;
}

static bool new_draft(struct app *a, const char *directory) {
    for (int attempt = 0; attempt < 8; attempt++) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        char name[96], path[PATH_MAX], lock_path[PATH_MAX];
        snprintf(name, sizeof name, "draft-%ld-%ld-%d.json",
                 (long)getpid(), now.tv_nsec, attempt);
        if (!path_join(path, sizeof path, directory, name)) return false;
        int fd = open_draft_lock(path, true, lock_path, sizeof lock_path);
        if (fd < 0) continue;
        snprintf(a->draft, sizeof a->draft, "%s", path);
        snprintf(a->draft_lock_path, sizeof a->draft_lock_path, "%s", lock_path);
        a->draft_lock = fd;
        return true;
    }
    return false;
}

static bool save_draft(struct app *a) {
    if (!a->dirty || !a->text || !a->draft[0] || a->draft_lock < 0) return false;
    struct json_object *record = json_object_new_object();
    json_object_object_add(record, "path", a->file[0] ? json_object_new_string(a->file) : NULL);
    json_object_object_add(record, "text", json_object_new_string_len(a->text, (int)a->length));
    const char *data = json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN);
    bool saved = atomic_file(a->draft, data, strlen(data));
    if (!saved)
        snprintf(a->notice, sizeof a->notice, "Draft could not be saved");
    json_object_put(record);
    return saved;
}

static bool load_draft(struct app *a) {
    FILE *file = fopen(a->draft, "rb");
    if (!file) return false;
    struct stat info;
    if (stat(a->draft, &info) || info.st_size < 0 || info.st_size > MAX_FILE * 2 + 128) {
        fclose(file); return false;
    }
    char *data = malloc((size_t)info.st_size + 1);
    if (!data) { fclose(file); return false; }
    size_t length = fread(data, 1, (size_t)info.st_size, file);
    bool complete = !ferror(file) && length == (size_t)info.st_size;
    fclose(file);
    if (!complete) { free(data); return false; }
    data[length] = 0;
    struct json_object *record = json_tokener_parse(data);
    free(data);
    if (!record) return false;
    bool recovered = false;
    struct json_object *content = NULL, *path = NULL;
    if (json_object_object_get_ex(record, "text", &content) &&
        json_object_get_type(content) == json_type_string &&
        json_object_get_string_len(content) <= MAX_FILE &&
        !memchr(json_object_get_string(content), 0,
                (size_t)json_object_get_string_len(content)) &&
        g_utf8_validate(json_object_get_string(content),
                        json_object_get_string_len(content), NULL)) {
        char *restored = strdup(json_object_get_string(content));
        if (restored) {
            free(a->text);
            a->length = (size_t)json_object_get_string_len(content);
            a->text = restored;
            a->capacity = a->length + 1;
            a->cursor = a->length;
            a->anchor = a->cursor;
            clear_undo(a);
            a->dirty = true;
            a->file[0] = 0;
            if (json_object_object_get_ex(record, "path", &path) &&
                path && json_object_get_type(path) == json_type_string)
                snprintf(a->file, sizeof a->file, "%s", json_object_get_string(path));
            snprintf(a->notice, sizeof a->notice, "Recovered unsaved draft");
            recovered = true;
        }
    }
    json_object_put(record);
    return recovered;
}

static bool recover_draft(struct app *a, const char *directory) {
    DIR *stream = opendir(directory);
    if (!stream) return false;
    struct timespec newest = {0};
    char chosen[PATH_MAX] = "", chosen_lock[PATH_MAX] = "";
    int chosen_fd = -1;
    struct dirent *entry;
    while ((entry = readdir(stream))) {
        size_t length = strlen(entry->d_name);
        if (length < 12 || strncmp(entry->d_name, "draft-", 6) ||
            strcmp(entry->d_name + length - 5, ".json")) continue;
        char path[PATH_MAX], lock_path[PATH_MAX];
        struct stat info;
        if (!path_join(path, sizeof path, directory, entry->d_name) ||
            lstat(path, &info) || !S_ISREG(info.st_mode) ||
            info.st_uid != getuid() || (info.st_mode & 077) ||
            (chosen_fd >= 0 &&
             (info.st_mtim.tv_sec < newest.tv_sec ||
              (info.st_mtim.tv_sec == newest.tv_sec &&
               info.st_mtim.tv_nsec <= newest.tv_nsec)))) continue;
        int fd = open_draft_lock(path, false, lock_path, sizeof lock_path);
        if (fd < 0) continue;
        snprintf(a->draft, sizeof a->draft, "%s", path);
        if (!load_draft(a)) { close(fd); continue; }
        if (chosen_fd >= 0) close(chosen_fd);
        chosen_fd = fd;
        newest = info.st_mtim;
        snprintf(chosen, sizeof chosen, "%s", path);
        snprintf(chosen_lock, sizeof chosen_lock, "%s", lock_path);
    }
    closedir(stream);
    if (chosen_fd < 0) { a->draft[0] = 0; return false; }
    a->draft_lock = chosen_fd;
    snprintf(a->draft, sizeof a->draft, "%s", chosen);
    snprintf(a->draft_lock_path, sizeof a->draft_lock_path, "%s", chosen_lock);
    return true;
}

static void prepare_draft(struct app *a, bool restore) {
    char local[PATH_MAX], state[PATH_MAX], heurism[PATH_MAX], drafts[PATH_MAX];
    if (!path_join(local, sizeof local, a->home, ".local") ||
        !path_join(state, sizeof state, local, "state") ||
        !path_join(heurism, sizeof heurism, state, "heurism") ||
        !path_join(drafts, sizeof drafts, heurism, "drafts") ||
        !ensure_directory(local, 0700) || !ensure_directory(state, 0700) ||
        !ensure_directory(heurism, 0700) || !ensure_directory(drafts, 0700)) {
        snprintf(a->notice, sizeof a->notice, "Draft autosave unavailable");
        return;
    }
    if (restore && recover_draft(a, drafts)) return;
    if (!new_draft(a, drafts)) {
        snprintf(a->notice, sizeof a->notice, "Draft autosave unavailable");
        return;
    }
    if (restore) {
        char legacy[PATH_MAX], current[PATH_MAX];
        struct stat info;
        if (path_join(legacy, sizeof legacy, heurism, "native-editor-draft.json") &&
            !lstat(legacy, &info) && S_ISREG(info.st_mode) &&
            info.st_uid == getuid() && !(info.st_mode & 077)) {
            snprintf(current, sizeof current, "%s", a->draft);
            snprintf(a->draft, sizeof a->draft, "%s", legacy);
            bool loaded = load_draft(a);
            snprintf(a->draft, sizeof a->draft, "%s", current);
            if (loaded && save_draft(a)) unlink(legacy);
        }
    }
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
    if (a->undo_count) a->undo[a->undo_count - 1].when = 0;
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
        if (a->mode == FILES) {
            if (a->prompt_text[0] == '/') navigate(a, a->prompt_text);
            else if (path_join(target, sizeof target, a->directory, a->prompt_text))
                navigate(a, target);
        } else if (editor_path(a, target, sizeof target)) {
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
        add_button(a, "Up", UP, 54);
        add_button(a, "New folder", CREATE, 110);
        add_button(a, "Hidden", TOGGLE_HIDDEN, 82);
        add_button(a, "Refresh", REFRESH, 86);
        add_button(a, "Go to location", LOCATION, 132);
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
    case DOCUMENTS:
    case DOWNLOADS:
    case PICTURES:
        if (path_join(path, sizeof path, a->home,
                      action == DOCUMENTS ? "Documents" :
                      action == DOWNLOADS ? "Downloads" : "Pictures")) navigate(a, path);
        break;
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
    case TOGGLE_HIDDEN:
        a->show_hidden = !a->show_hidden;
        list_directory(a);
        break;
    case LOCATION: prompt_start(a, OPEN_PATH, a->directory); break;
    case TRASH_BIN:
        if (path_join(path, sizeof path, a->home, ".local/share/heurism/trash")) {
            char local[PATH_MAX], share[PATH_MAX], heurism[PATH_MAX];
            if (path_join(local, sizeof local, a->home, ".local") &&
                path_join(share, sizeof share, local, "share") &&
                path_join(heurism, sizeof heurism, share, "heurism") &&
                ensure_directory(local, 0700) && ensure_directory(share, 0700) &&
                ensure_directory(heurism, 0700) &&
                ensure_directory(path, 0700)) navigate(a, path);
        }
        break;
    case NEW:
        if (a->dirty) snprintf(a->notice, sizeof a->notice, "Save this draft before creating another document");
        else {
            a->length = a->cursor = 0;
            a->anchor = 0;
            a->text[0] = a->file[0] = 0;
            a->scroll_line = a->scroll_column = 0;
            clear_undo(a);
        }
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

static bool replace_bytes(struct app *a, size_t start, size_t removed,
                          const char *inserted, size_t added) {
    if (start > a->length || removed > a->length - start ||
        added > MAX_FILE - (a->length - removed)) {
        snprintf(a->notice, sizeof a->notice, "Document size limit reached");
        return false;
    }
    size_t new_length = a->length - removed + added;
    if (new_length + 1 > a->capacity) {
        size_t capacity = a->capacity ? a->capacity : 64;
        while (capacity < new_length + 1 && capacity < MAX_FILE + 1)
            capacity = capacity > (MAX_FILE + 1) / 2 ? MAX_FILE + 1 : capacity * 2;
        char *larger = realloc(a->text, capacity);
        if (!larger) {
            snprintf(a->notice, sizeof a->notice, "Out of memory");
            return false;
        }
        a->text = larger;
        a->capacity = capacity;
    }
    memmove(a->text + start + added, a->text + start + removed,
            a->length - start - removed + 1);
    if (added) memcpy(a->text + start, inserted, added);
    a->length = new_length;
    a->cursor = start + added;
    a->anchor = a->cursor;
    a->dirty = true;
    return true;
}

static void edit_text(struct app *a, size_t start, size_t end,
                      const char *inserted, size_t added) {
    if (start > end || end > a->length || (!added && start == end)) return;
    struct edit change = {.start = start, .removed_len = end - start,
                          .inserted_len = added, .cursor_before = a->cursor,
                          .when = time(NULL)};
    if (change.removed_len) {
        change.removed = malloc(change.removed_len);
        if (change.removed) memcpy(change.removed, a->text + start, change.removed_len);
    }
    if (added) {
        change.inserted = malloc(added);
        if (change.inserted) memcpy(change.inserted, inserted, added);
    }
    if ((change.removed_len && !change.removed) || (added && !change.inserted)) {
        free_edit(&change);
        snprintf(a->notice, sizeof a->notice, "Out of memory");
        return;
    }
    if (!replace_bytes(a, start, change.removed_len, inserted, added)) {
        free_edit(&change);
        return;
    }
    if (added && !change.removed_len && a->undo_position == a->undo_count &&
        a->undo_count && change.when > 0) {
        struct edit *previous = &a->undo[a->undo_count - 1];
        bool plain = true;
        for (size_t i = 0; i < added; i++)
            if (inserted[i] == ' ' || inserted[i] == '\t' || inserted[i] == '\n') plain = false;
        if (plain && !previous->removed_len && previous->inserted_len &&
            previous->start + previous->inserted_len == start &&
            previous->inserted_len + added <= 512 && previous->when > 0 &&
            change.when >= previous->when && change.when - previous->when <= 2) {
            char *merged = realloc(previous->inserted, previous->inserted_len + added);
            if (merged) {
                previous->inserted = merged;
                memcpy(previous->inserted + previous->inserted_len, inserted, added);
                previous->inserted_len += added;
                previous->when = change.when;
                free_edit(&change);
                a->notice[0] = 0;
                return;
            }
        }
    }
    for (int i = a->undo_position; i < a->undo_count; i++) free_edit(&a->undo[i]);
    a->undo_count = a->undo_position;
    if (a->undo_count == MAX_UNDO) {
        free_edit(&a->undo[0]);
        memmove(a->undo, a->undo + 1, (MAX_UNDO - 1) * sizeof a->undo[0]);
        a->undo_count--;
        a->undo_position--;
    }
    a->undo[a->undo_count++] = change;
    a->undo_position = a->undo_count;
    a->notice[0] = 0;
}

static void insert_text(struct app *a, const char *bytes, size_t count) {
    size_t start = a->anchor < a->cursor ? a->anchor : a->cursor;
    size_t end = a->anchor > a->cursor ? a->anchor : a->cursor;
    edit_text(a, start, end, bytes, count);
}

static void delete_range(struct app *a, size_t start, size_t end) {
    edit_text(a, start, end, NULL, 0);
}

static void undo_edit(struct app *a, bool redo) {
    if ((!redo && !a->undo_position) || (redo && a->undo_position == a->undo_count))
        return;
    struct edit *change = &a->undo[redo ? a->undo_position : a->undo_position - 1];
    if (replace_bytes(a, change->start,
                      redo ? change->removed_len : change->inserted_len,
                      redo ? change->inserted : change->removed,
                      redo ? change->inserted_len : change->removed_len)) {
        a->cursor = redo ? change->start + change->inserted_len : change->cursor_before;
        a->anchor = a->cursor;
        a->undo_position += redo ? 1 : -1;
        snprintf(a->notice, sizeof a->notice, redo ? "Redone" : "Undone");
    }
}

static void ensure_cursor_visible(struct app *a);

static bool selected_range(struct app *a, size_t *start, size_t *end) {
    *start = a->anchor < a->cursor ? a->anchor : a->cursor;
    *end = a->anchor > a->cursor ? a->anchor : a->cursor;
    return *start < *end;
}

static bool own_selection(struct app *a, Atom selection, Time time) {
    size_t start, end;
    if (!selected_range(a, &start, &end)) return false;
    char *copy = malloc(end - start + 1);
    if (!copy) {
        snprintf(a->notice, sizeof a->notice, "Out of memory");
        return false;
    }
    memcpy(copy, a->text + start, end - start);
    copy[end - start] = 0;
    char **slot = selection == XA_PRIMARY ? &a->primary_text : &a->clipboard_text;
    size_t *length = selection == XA_PRIMARY ? &a->primary_length : &a->clipboard_length;
    bool *owned = selection == XA_PRIMARY ? &a->owns_primary : &a->owns_clipboard;
    free(*slot);
    *slot = copy;
    *length = end - start;
    XSetSelectionOwner(a->display, selection, a->window, time);
    *owned = XGetSelectionOwner(a->display, selection) == a->window;
    if (!*owned) snprintf(a->notice, sizeof a->notice, "Clipboard unavailable");
    return *owned;
}

static void change_text_property(struct app *a, Window requestor, Atom property,
                                 Atom type, const char *value, size_t length) {
    size_t offset = 0;
    do {
        size_t chunk = length - offset;
        if (chunk > 65536) chunk = 65536;
        XChangeProperty(a->display, requestor, property, type, 8,
                        offset ? PropModeAppend : PropModeReplace,
                        (const unsigned char *)value + offset, (int)chunk);
        offset += chunk;
    } while (offset < length);
}

static void selection_request(struct app *a, XSelectionRequestEvent *request) {
    XSelectionEvent reply = {.type = SelectionNotify, .display = a->display,
        .requestor = request->requestor, .selection = request->selection,
        .target = request->target, .property = None, .time = request->time};
    bool primary = request->selection == XA_PRIMARY;
    bool owned = primary ? a->owns_primary :
                 request->selection == a->clipboard && a->owns_clipboard;
    const char *value = primary ? a->primary_text : a->clipboard_text;
    size_t length = primary ? a->primary_length : a->clipboard_length;
    Atom property = request->property == None ? request->target : request->property;
    if (owned && value) {
        if (request->target == a->targets) {
            Atom formats[] = {a->targets, a->utf8, XA_STRING};
            XChangeProperty(a->display, request->requestor, property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)formats, 3);
            reply.property = property;
        } else if (request->target == a->utf8) {
            change_text_property(a, request->requestor, property,
                                 a->utf8, value, length);
            reply.property = property;
        } else if (request->target == XA_STRING) {
            gsize converted_length = 0;
            char *converted = g_convert_with_fallback(value, (gssize)length,
                "ISO-8859-1", "UTF-8", "?", NULL, &converted_length, NULL);
            if (converted) {
                change_text_property(a, request->requestor, property,
                                     XA_STRING, converted, converted_length);
                reply.property = property;
                g_free(converted);
            }
        }
    }
    XSendEvent(a->display, request->requestor, False, 0, (XEvent *)&reply);
    XFlush(a->display);
}

static void cancel_paste(struct app *a, const char *notice) {
    a->pending_paste = None;
    a->paste_incr = false;
    free(a->paste_text);
    a->paste_text = NULL;
    a->paste_length = 0;
    XDeleteProperty(a->display, a->window, a->paste_property);
    if (notice) snprintf(a->notice, sizeof a->notice, "%s", notice);
}

static bool append_paste(struct app *a, const unsigned char *data, size_t length) {
    if (length > MAX_FILE - a->paste_length) return false;
    char *larger = realloc(a->paste_text, a->paste_length + length + 1);
    if (!larger) return false;
    a->paste_text = larger;
    if (length) memcpy(a->paste_text + a->paste_length, data, length);
    a->paste_length += length;
    a->paste_text[a->paste_length] = 0;
    return true;
}

static void finish_paste(struct app *a) {
    char *content = a->paste_text;
    size_t length = a->paste_length;
    char *converted = NULL;
    if (a->paste_target == XA_STRING) {
        gsize converted_length = 0;
        converted = g_convert(content, (gssize)length, "UTF-8", "ISO-8859-1",
                              NULL, &converted_length, NULL);
        content = converted;
        length = converted_length;
    }
    if (!content || length > MAX_FILE || memchr(content, 0, length) ||
        !g_utf8_validate(content, (gssize)length, NULL)) {
        cancel_paste(a, "Paste rejected: invalid or oversized text");
    } else {
        edit_text(a, a->paste_start, a->paste_end, content, length);
        cancel_paste(a, NULL);
        ensure_cursor_visible(a);
    }
    g_free(converted);
}

static void request_paste(struct app *a, Atom selection, Time event_time) {
    if (a->pending_paste != None) return;
    size_t start, end;
    selected_range(a, &start, &end);
    a->paste_start = start;
    a->paste_end = end;
    a->pending_paste = selection;
    a->paste_target = a->utf8;
    a->paste_deadline = time(NULL) + 10;
    XConvertSelection(a->display, selection, a->paste_target,
                      a->paste_property, a->window, event_time);
}

static void selection_notify(struct app *a, XSelectionEvent *event) {
    if (event->selection != a->pending_paste) return;
    if (event->property == None) {
        if (a->paste_target == a->utf8) {
            a->paste_target = XA_STRING;
            XConvertSelection(a->display, event->selection, XA_STRING,
                              a->paste_property, a->window, event->time);
        } else cancel_paste(a, "Clipboard has no text");
        return;
    }
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    int result = XGetWindowProperty(a->display, a->window, a->paste_property,
                                    0, (MAX_FILE + 3) / 4, True, AnyPropertyType,
                                    &type, &format, &count, &remaining, &data);
    if (result == Success && type == a->incr && format == 32) {
        a->paste_incr = true;
        XDeleteProperty(a->display, a->window, a->paste_property);
    } else if (result == Success && format == 8 && !remaining &&
               (type == a->utf8 || type == XA_STRING) &&
               append_paste(a, data, count)) {
        a->paste_target = type;
        finish_paste(a);
    } else cancel_paste(a, "Paste rejected: invalid or oversized text");
    if (data) XFree(data);
}

static void paste_property_notify(struct app *a, XPropertyEvent *event) {
    if (!a->paste_incr || event->atom != a->paste_property ||
        event->state != PropertyNewValue) return;
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    int result = XGetWindowProperty(a->display, a->window, a->paste_property,
                                    0, (MAX_FILE + 3) / 4, True, AnyPropertyType,
                                    &type, &format, &count, &remaining, &data);
    if (result != Success || format != 8 || remaining ||
        (type != a->utf8 && type != XA_STRING) ||
        !append_paste(a, data, count))
        cancel_paste(a, "Paste rejected: invalid or oversized text");
    else if (!count) {
        a->paste_target = type;
        finish_paste(a);
    } else a->paste_deadline = time(NULL) + 10;
    if (data) XFree(data);
}

static int cursor_line(struct app *a) {
    int line = 0;
    for (size_t i = 0; i < a->cursor; i++) if (a->text[i] == '\n') line++;
    return line;
}

static size_t line_start(const char *buffer, size_t offset) {
    while (offset && buffer[offset - 1] != '\n') offset--;
    return offset;
}

static size_t line_end(const char *buffer, size_t length, size_t start) {
    while (start < length && buffer[start] != '\n') start++;
    return start;
}

static int character_column(const char *buffer, size_t start, size_t offset) {
    int column = 0;
    while (start < offset) { start = next_character(buffer, offset, start); column++; }
    return column;
}

static size_t advance_columns(const char *buffer, size_t end, size_t start, int columns) {
    while (columns-- > 0 && start < end) start = next_character(buffer, end, start);
    return start;
}

static void ensure_cursor_visible(struct app *a) {
    int line = cursor_line(a);
    int rows = (a->height - EDITOR_ROW_TOP - 56) / EDITOR_ROW_HEIGHT;
    if (rows < 1) rows = 1;
    if (line < a->scroll_line) a->scroll_line = line;
    if (line >= a->scroll_line + rows) a->scroll_line = line - rows + 1;
    if (a->scroll_line < 0) a->scroll_line = 0;
    int width = a->font ? a->font->max_advance_width : 12;
    if (width < 1) width = 12;
    int columns = (a->width - 108) / width;
    if (columns < 1) columns = 1;
    int column = character_column(a->text, line_start(a->text, a->cursor), a->cursor);
    if (column < a->scroll_column) a->scroll_column = column;
    if (column >= a->scroll_column + columns)
        a->scroll_column = column - columns + 1;
}

static void move_vertical(struct app *a, int delta) {
    size_t start = line_start(a->text, a->cursor);
    int column = character_column(a->text, start, a->cursor);
    if (delta < 0) {
        if (!start) return;
        size_t previous_end = start - 1, previous_start = previous_end;
        while (previous_start && a->text[previous_start - 1] != '\n') previous_start--;
        a->cursor = advance_columns(a->text, previous_end, previous_start, column);
    } else {
        size_t end = line_end(a->text, a->length, a->cursor);
        if (end == a->length) return;
        size_t next_start = end + 1, next_end = next_start;
        while (next_end < a->length && a->text[next_end] != '\n') next_end++;
        a->cursor = advance_columns(a->text, next_end, next_start, column);
    }
    ensure_cursor_visible(a);
}

static bool place_active(struct app *a, enum command action) {
    char path[PATH_MAX];
    if (action == HOME) return !strcmp(a->directory, a->home);
    if (action == TRASH_BIN) return is_trash(a);
    const char *name = action == DOCUMENTS ? "Documents" :
                       action == DOWNLOADS ? "Downloads" : "Pictures";
    return path_join(path, sizeof path, a->home, name) &&
           !strcmp(a->directory, path);
}

static void entry_icon(struct app *a, int x, int y, bool directory) {
    if (directory) {
        box(a, x, y + 5, 23, 16, 60, 118, 163);
        box(a, x + 2, y + 2, 10, 5, 91, 151, 193);
    } else {
        box(a, x + 3, y, 18, 23, 69, 102, 126);
        box(a, x + 7, y + 6, 10, 2, 203, 223, 232);
        box(a, x + 7, y + 11, 10, 2, 203, 223, 232);
    }
}

static void render_files(struct app *a) {
    box(a, 0, 0, a->width, a->height, 13, 22, 36);
    box(a, 0, 0, a->width, 112, 25, 37, 54);
    text(a, 24, 43, "Files", a->font_title, 240, 247, 250);
    for (int i = 0; i < a->button_count; i++) {
        struct button *button = &a->buttons[i];
        bool selected = button->command == TOGGLE_HIDDEN && a->show_hidden;
        round_box(a, button->x, 61, button->width, 41, 7,
                  selected ? 58 : 38, selected ? 112 : 57, selected ? 120 : 76);
        text(a, button->x + 10, 87, button->label, a->font_small, 231, 241, 247);
    }
    box(a, 0, 112, 207, a->height - 155, 20, 31, 47);
    text(a, 26, 151, "PLACES", a->font_small, 138, 166, 184);
    const struct { const char *name; enum command action; } places[] = {
        {"Home", HOME}, {"Documents", DOCUMENTS}, {"Downloads", DOWNLOADS},
        {"Pictures", PICTURES}, {"Trash", TRASH_BIN}
    };
    for (size_t i = 0; i < sizeof places / sizeof places[0]; i++) {
        int y = 174 + (int)i * 44;
        if (place_active(a, places[i].action))
            round_box(a, 16, y, 175, 39, 7, 39, 76, 84);
        box(a, 31, y + 15, 10, 10, 83, 182, 178);
        text(a, 53, y + 26, places[i].name, a->font_small,
             223, 236, 243);
    }
    box(a, 207, 112, 1, a->height - 155, 44, 62, 80);
    const char *leaf = strrchr(a->directory, '/');
    leaf = leaf && leaf[1] ? leaf + 1 : a->directory;
    text(a, 230, 146, leaf, a->font_title, 239, 246, 250);
    XRectangle path_clip = {230, 151, (unsigned short)(a->width - 255), 31};
    XftDrawSetClipRectangles(a->draw, 0, 0, &path_clip, 1);
    text(a, 230, 173, a->directory, a->font_small, 146, 171, 190);
    XftDrawSetClip(a->draw, NULL);
    box(a, 226, 185, a->width - 248, 1, 46, 64, 81);
    int visible = (a->height - FILE_ROW_TOP - 105) / FILE_ROW_HEIGHT;
    if (visible < 1) visible = 1;
    if (!a->entry_count)
        text(a, 272, 246, "This folder is empty", a->font_small, 146, 171, 190);
    for (int row = 0; row < visible && row + a->scroll < a->entry_count; row++) {
        int index = row + a->scroll;
        int y = FILE_ROW_TOP + row * FILE_ROW_HEIGHT;
        struct entry *entry = &a->entries[index];
        round_box(a, 220, y, a->width - 236, FILE_ROW_HEIGHT - 2, 6,
                  index == a->selected ? 37 : row % 2 ? 19 : 23,
                  index == a->selected ? 75 : row % 2 ? 32 : 37,
                  index == a->selected ? 86 : row % 2 ? 49 : 54);
        entry_icon(a, 235, y + 8, entry->directory);
        XRectangle name_clip = {270, (short)y, (unsigned short)(a->width - 435),
                                FILE_ROW_HEIGHT};
        XftDrawSetClipRectangles(a->draw, 0, 0, &name_clip, 1);
        text(a, 272, y + 27, entry->name, a->font_small, 235, 243, 247);
        XftDrawSetClip(a->draw, NULL);
        char details[48];
        if (entry->directory) snprintf(details, sizeof details, "Folder");
        else if (entry->size >= 1048576)
            snprintf(details, sizeof details, "%lld MiB", (long long)(entry->size / 1048576));
        else if (entry->size >= 1024)
            snprintf(details, sizeof details, "%lld KiB", (long long)(entry->size / 1024));
        else snprintf(details, sizeof details, "%lld B", (long long)entry->size);
        text(a, a->width - 144, y + 27, details, a->font_small, 146, 171, 190);
    }
    box(a, 208, a->height - 99, a->width - 208, 56, 25, 37, 54);
    box(a, 208, a->height - 99, a->width - 208, 1, 49, 67, 83);
    if (a->selected >= 0 && a->selected < a->entry_count) {
        XRectangle selected_clip = {230, (short)(a->height - 94),
                                    (unsigned short)(a->width - 590), 48};
        XftDrawSetClipRectangles(a->draw, 0, 0, &selected_clip, 1);
        text(a, 230, a->height - 65, a->entries[a->selected].name,
             a->font_small, 231, 242, 247);
        XftDrawSetClip(a->draw, NULL);
        int actions_x = a->width - 342;
        round_box(a, actions_x, a->height - 91, 76, 40, 7, 42, 68, 86);
        text(a, actions_x + 15, a->height - 65, "Open", a->font_small,
             236, 246, 249);
        if (!is_trash(a)) {
            round_box(a, actions_x + 84, a->height - 91, 90, 40, 7, 42, 68, 86);
            text(a, actions_x + 96, a->height - 65, "Rename", a->font_small,
                 236, 246, 249);
        }
        round_box(a, actions_x + 182, a->height - 91, 130, 40, 7, 42, 68, 86);
        text(a, actions_x + 193, a->height - 65,
             is_trash(a) ? "Restore" : "Move to Trash", a->font_small,
             236, 246, 249);
    } else text(a, 230, a->height - 66, "Select an item to open or manage it",
                a->font_small, 146, 171, 190);
    box(a, 0, a->height - 43, a->width, 43, 25, 37, 54);
    if (a->prompt != NO_PROMPT) {
        char line[PATH_MAX + 64];
        const char *title = a->prompt == NEW_FOLDER ? "New folder" :
                            a->prompt == RENAME ? "New name" :
                            a->prompt == TRASH_CONFIRM ? "Type yes to move to Trash" :
                            "Location";
        snprintf(line, sizeof line, "%s: %s_", title, a->prompt_text);
        text(a, 24, a->height - 15, line, a->font_small, 80, 225, 190);
    } else if (a->notice[0])
        text(a, 24, a->height - 15, a->notice, a->font_small, 80, 225, 190);
    else {
        char count[72];
        snprintf(count, sizeof count, "%d item%s  ·  Ctrl+L location  ·  Ctrl+H hidden",
                 a->entry_count, a->entry_count == 1 ? "" : "s");
        text(a, 24, a->height - 15, count, a->font_small, 146, 171, 190);
    }
}

static void render_editor(struct app *a) {
    box(a, 0, 0, a->width, a->height, 13, 22, 36);
    box(a, 0, 0, a->width, 112, 25, 37, 54);
    text(a, 24, 43, "Editor", a->font_title, 240, 247, 250);
    for (int i = 0; i < a->button_count; i++) {
        struct button *button = &a->buttons[i];
        bool save = button->command == SAVE && a->dirty;
        round_box(a, button->x, 61, button->width, 41, 7,
                  save ? 54 : 38, save ? 126 : 57, save ? 117 : 76);
        text(a, button->x + 10, 87, button->label, a->font_small, 231, 241, 247);
    }
    const char *name = a->file[0] ? strrchr(a->file, '/') : NULL;
    name = name && name[1] ? name + 1 : a->file[0] ? a->file : "Untitled document";
    XRectangle title_clip = {24, 113, (unsigned short)(a->width - 160), 40};
    XftDrawSetClipRectangles(a->draw, 0, 0, &title_clip, 1);
    text(a, 24, 146, name, a->font_title, 239, 246, 250);
    XftDrawSetClip(a->draw, NULL);
    XRectangle path_clip = {24, 151, (unsigned short)(a->width - 48), 31};
    XftDrawSetClipRectangles(a->draw, 0, 0, &path_clip, 1);
    text(a, 24, 173, a->file[0] ? a->file : "Save to choose a location",
         a->font_small, 146, 171, 190);
    XftDrawSetClip(a->draw, NULL);
    if (a->dirty) {
        round_box(a, a->width - 112, 119, 88, 32, 7, 39, 76, 84);
        text(a, a->width - 100, 140, "Unsaved", a->font_small, 137, 236, 214);
    }
    box(a, 0, 185, a->width, 1, 46, 64, 81);
    box(a, 0, 186, 65, a->height - 229, 20, 31, 47);
    box(a, 65, 186, 1, a->height - 229, 44, 62, 80);
    int visible = (a->height - EDITOR_ROW_TOP - 56) / EDITOR_ROW_HEIGHT;
    if (visible < 1) visible = 1;
    size_t offset = 0;
    for (int line = 0; line < a->scroll_line && offset < a->length; line++) {
        offset = line_end(a->text, a->length, offset);
        if (offset < a->length) offset++;
    }
    XRectangle content_clip = {80, EDITOR_ROW_TOP, (unsigned short)(a->width - 96),
                               (unsigned short)(a->height - EDITOR_ROW_TOP - 51)};
    size_t selection_start, selection_end;
    bool highlighted = selected_range(a, &selection_start, &selection_end);
    for (int row = 0; row < visible && offset <= a->length; row++) {
        size_t end = line_end(a->text, a->length, offset);
        int y = EDITOR_ROW_TOP + row * EDITOR_ROW_HEIGHT;
        if (a->cursor >= offset && a->cursor <= end)
            box(a, 66, y, a->width - 66, EDITOR_ROW_HEIGHT, 20, 36, 52);
        char number[24];
        snprintf(number, sizeof number, "%d", a->scroll_line + row + 1);
        text(a, 21, y + 22, number, a->font_small, 115, 143, 163);
        XftDrawSetClipRectangles(a->draw, 0, 0, &content_clip, 1);
        size_t visible_start = advance_columns(a->text, end, offset, a->scroll_column);
        size_t visible_end = advance_columns(a->text, end, visible_start, 256);
        char content[1025];
        size_t count = visible_end - visible_start;
        if (count > sizeof content - 1) count = sizeof content - 1;
        while (count && !g_utf8_validate(a->text + visible_start, (gssize)count, NULL))
            count--;
        memcpy(content, a->text + visible_start, count);
        content[count] = 0;
        if (highlighted) {
            size_t first = selection_start > visible_start ? selection_start : visible_start;
            size_t last = selection_end < visible_start + count ?
                          selection_end : visible_start + count;
            if (first < last) {
                XGlyphInfo before, through;
                XftTextExtentsUtf8(a->display, a->font, (FcChar8 *)content,
                                   (int)(first - visible_start), &before);
                XftTextExtentsUtf8(a->display, a->font, (FcChar8 *)content,
                                   (int)(last - visible_start), &through);
                int left = 84 + before.xOff, right = 84 + through.xOff;
                if (left < 80) left = 80;
                if (right > a->width - 16) right = a->width - 16;
                box(a, left, y + 2, right - left, EDITOR_ROW_HEIGHT - 4,
                    43, 117, 115);
            }
        }
        text(a, 84, y + 22, content, a->font, 235, 243, 247);
        if (a->cursor >= visible_start && a->cursor <= visible_end &&
            a->cursor >= offset && a->cursor <= end) {
            XGlyphInfo extent;
            XftTextExtentsUtf8(a->display, a->font, (FcChar8 *)content,
                               (int)(a->cursor - visible_start), &extent);
            box(a, 84 + extent.xOff, y + 4, 2, 24, 80, 225, 190);
        }
        XftDrawSetClip(a->draw, NULL);
        if (end == a->length) break;
        offset = end + 1;
    }
    box(a, 0, a->height - 43, a->width, 43, 25, 37, 54);
    XRectangle footer_clip = {24, (short)(a->height - 41),
                              (unsigned short)(a->width - 48), 40};
    XftDrawSetClipRectangles(a->draw, 0, 0, &footer_clip, 1);
    if (a->prompt != NO_PROMPT) {
        char line[PATH_MAX + 64];
        snprintf(line, sizeof line, "%s: %s_",
                 a->prompt == OPEN_PATH ? "Open path" : "Save path", a->prompt_text);
        text(a, 24, a->height - 15, line, a->font_small, 80, 225, 190);
    } else {
        char status[128];
        int column = character_column(a->text, line_start(a->text, a->cursor), a->cursor);
        if (highlighted)
            snprintf(status, sizeof status,
                     "Text selected  ·  Ctrl+C copy  ·  Ctrl+X cut  ·  Ctrl+V paste");
        else
            snprintf(status, sizeof status,
                     "Ln %d, Col %d  ·  UTF-8  ·  Ctrl+Z undo  ·  Ctrl+Y redo",
                     cursor_line(a) + 1, column + 1);
        if (a->notice[0] && a->width < 850)
            text(a, 24, a->height - 15, a->notice, a->font_small, 80, 225, 190);
        else {
            text(a, 24, a->height - 15, status, a->font_small, 146, 171, 190);
            if (a->notice[0]) text(a, a->width - 180, a->height - 15,
                                   a->notice, a->font_small, 80, 225, 190);
        }
    }
    XftDrawSetClip(a->draw, NULL);
}

static void render(struct app *a) {
    if (a->mode == FILES) render_files(a);
    else render_editor(a);
    XFlush(a->display);
}

static void editor_cursor_from_point(struct app *a, int x, int y) {
    int row = (y - EDITOR_ROW_TOP) / EDITOR_ROW_HEIGHT;
    if (row < 0) row = 0;
    int target_line = a->scroll_line + row;
    size_t start = 0;
    for (int line = 0; line < target_line && start < a->length; line++) {
        size_t end = line_end(a->text, a->length, start);
        start = end < a->length ? end + 1 : a->length;
    }
    size_t end = line_end(a->text, a->length, start);
    size_t visible = advance_columns(a->text, end, start, a->scroll_column);
    a->cursor = visible;
    if (x > 84) {
        size_t position = visible;
        while (position < end) {
            size_t next = next_character(a->text, end, position);
            XGlyphInfo extent;
            XftTextExtentsUtf8(a->display, a->font, (FcChar8 *)(a->text + visible),
                               (int)(next - visible), &extent);
            if (x < 84 + extent.xOff - a->font->max_advance_width / 2) break;
            a->cursor = next;
            position = next;
            if (extent.xOff > a->width - 96) break;
        }
    }
    ensure_cursor_visible(a);
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
        if ((event->state & ControlMask) && (symbol == XK_h || symbol == XK_H))
            command(a, TOGGLE_HIDDEN);
        else if ((event->state & ControlMask) && (symbol == XK_l || symbol == XK_L))
            command(a, LOCATION);
        else if ((event->state & ControlMask) && (event->state & ShiftMask) &&
                 (symbol == XK_n || symbol == XK_N)) command(a, CREATE);
        else if ((event->state & Mod1Mask) && symbol == XK_Up) command(a, UP);
        else if (symbol == XK_Return) open_selected(a);
        else if (symbol == XK_Up && a->selected > 0) a->selected--;
        else if (symbol == XK_Down && a->selected + 1 < a->entry_count) a->selected++;
        else if (symbol == XK_Delete) command(a, TRASH);
        else if (symbol == XK_F2) command(a, CHANGE_NAME);
        if (a->selected < a->scroll) a->scroll = a->selected;
        int visible = (a->height - FILE_ROW_TOP - 105) / FILE_ROW_HEIGHT;
        if (visible < 1) visible = 1;
        if (a->selected >= a->scroll + visible) a->scroll = a->selected - visible + 1;
        if (a->scroll < 0) a->scroll = 0;
        return;
    }
    if (a->pending_paste != None) {
        if (symbol == XK_Escape) cancel_paste(a, "Paste cancelled");
        return;
    }
    bool shift = (event->state & ShiftMask) != 0;
    if (event->state & ControlMask) {
        if (symbol == XK_s || symbol == XK_S)
            command(a, shift ? SAVE_AS : SAVE);
        else if (symbol == XK_o || symbol == XK_O) command(a, OPEN);
        else if (symbol == XK_n || symbol == XK_N) command(a, NEW);
        else if (symbol == XK_z || symbol == XK_Z) undo_edit(a, shift);
        else if (symbol == XK_y || symbol == XK_Y) undo_edit(a, true);
        else if (symbol == XK_a || symbol == XK_A) {
            a->anchor = 0;
            a->cursor = a->length;
            own_selection(a, XA_PRIMARY, event->time);
        } else if (symbol == XK_c || symbol == XK_C) {
            if (own_selection(a, a->clipboard, event->time))
                snprintf(a->notice, sizeof a->notice, "Copied to clipboard");
            else snprintf(a->notice, sizeof a->notice, "Select text to copy");
        } else if (symbol == XK_x || symbol == XK_X) {
            size_t start, end;
            if (selected_range(a, &start, &end) &&
                own_selection(a, a->clipboard, event->time)) {
                delete_range(a, start, end);
                snprintf(a->notice, sizeof a->notice, "Cut to clipboard");
            } else snprintf(a->notice, sizeof a->notice, "Select text to cut");
        } else if (symbol == XK_v || symbol == XK_V)
            request_paste(a, a->clipboard, event->time);
        else if (symbol == XK_Home || symbol == XK_End) {
            a->cursor = symbol == XK_Home ? 0 : a->length;
            if (shift) own_selection(a, XA_PRIMARY, event->time);
            else a->anchor = a->cursor;
        }
        ensure_cursor_visible(a);
        return;
    }
    bool moving = true;
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
    case XK_Escape: a->anchor = a->cursor; break;
    case XK_BackSpace:
        moving = false;
        if (a->anchor != a->cursor) insert_text(a, NULL, 0);
        else if (a->cursor)
            delete_range(a, previous_character(a->text, a->cursor), a->cursor);
        break;
    case XK_Delete:
        moving = false;
        if (a->anchor != a->cursor) insert_text(a, NULL, 0);
        else if (a->cursor < a->length)
            delete_range(a, a->cursor,
                         next_character(a->text, a->length, a->cursor));
        break;
    case XK_Return: moving = false; insert_text(a, "\n", 1); break;
    case XK_Tab: moving = false; insert_text(a, "    ", 4); break;
    default:
        moving = false;
        if (count > 0 && (unsigned char)input[0] >= 32 && !memchr(input, 0, (size_t)count))
            insert_text(a, input, (size_t)count);
        break;
    }
    if (moving) {
        if (shift) own_selection(a, XA_PRIMARY, event->time);
        else a->anchor = a->cursor;
    }
    ensure_cursor_visible(a);
}

static void click(struct app *a, XButtonEvent *event) {
    if (a->mode == EDITOR && event->button == 2 && a->prompt == NO_PROMPT) {
        if (event->y >= EDITOR_ROW_TOP && event->y < a->height - 43) {
            editor_cursor_from_point(a, event->x, event->y);
            a->anchor = a->cursor;
        }
        request_paste(a, XA_PRIMARY, event->time);
        return;
    }
    if (event->button == 4 || event->button == 5) {
        if (a->mode == FILES) {
            a->scroll += event->button == 5 ? 3 : -3;
            if (a->scroll < 0) a->scroll = 0;
            if (a->scroll >= a->entry_count) a->scroll = a->entry_count ? a->entry_count - 1 : 0;
        } else {
            a->scroll_line += event->button == 5 ? 3 : -3;
            if (a->scroll_line < 0) a->scroll_line = 0;
            int lines = 0;
            for (size_t i = 0; i < a->length; i++) if (a->text[i] == '\n') lines++;
            if (a->scroll_line > lines) a->scroll_line = lines;
        }
        return;
    }
    if (event->button != 1) return;
    if (event->y >= 61 && event->y < 102) {
        for (int i = 0; i < a->button_count; i++) {
            struct button *button = &a->buttons[i];
            if (event->x >= button->x && event->x < button->x + button->width) {
                command(a, button->command); return;
            }
        }
    }
    if (a->mode == FILES && event->x >= 16 && event->x < 191 &&
        event->y >= 174 && event->y < 394) {
        int place = (event->y - 174) / 44;
        if ((event->y - 174) % 44 < 39) {
            static const enum command actions[] =
                {HOME, DOCUMENTS, DOWNLOADS, PICTURES, TRASH_BIN};
            command(a, actions[place]);
        }
        return;
    }
    if (a->mode == FILES && a->selected >= 0 &&
        event->y >= a->height - 91 && event->y < a->height - 51) {
        int x = a->width - 342;
        if (event->x >= x && event->x < x + 76) command(a, OPEN);
        else if (!is_trash(a) && event->x >= x + 84 && event->x < x + 174)
            command(a, CHANGE_NAME);
        else if (event->x >= x + 182 && event->x < x + 312)
            command(a, is_trash(a) ? RESTORE : TRASH);
        return;
    }
    if (a->mode == FILES && event->x >= 220 &&
        event->y >= FILE_ROW_TOP && event->y < a->height - 99) {
        int index = a->scroll + (event->y - FILE_ROW_TOP) / FILE_ROW_HEIGHT;
        if (index >= 0 && index < a->entry_count) {
            bool double_click = a->selected == index && event->time - a->last_click < 350;
            a->selected = index;
            a->last_click = event->time;
            if (double_click) open_selected(a);
        }
    }
    if (a->mode == EDITOR && a->prompt == NO_PROMPT &&
        a->pending_paste == None && event->y >= EDITOR_ROW_TOP &&
        event->y < a->height - 43) {
        editor_cursor_from_point(a, event->x, event->y);
        if (!(event->state & ShiftMask)) a->anchor = a->cursor;
        a->selecting = true;
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
    XSizeHints size = {.flags = PMinSize,
                       .min_width = a->mode == FILES ? 900 : 600,
                       .min_height = 480};
    XSetWMNormalHints(a->display, a->window, &size);
    XStoreName(a->display, a->window, a->mode == FILES ? "Heurism Files" : "Heurism Editor");
    XSelectInput(a->display, a->window, ExposureMask | ButtonPressMask |
                 ButtonReleaseMask | Button1MotionMask | PropertyChangeMask |
                 KeyPressMask | StructureNotifyMask | FocusChangeMask);
    a->delete_window = XInternAtom(a->display, "WM_DELETE_WINDOW", False);
    a->clipboard = XInternAtom(a->display, "CLIPBOARD", False);
    a->utf8 = XInternAtom(a->display, "UTF8_STRING", False);
    a->targets = XInternAtom(a->display, "TARGETS", False);
    a->incr = XInternAtom(a->display, "INCR", False);
    a->paste_property = XInternAtom(a->display, "HEURISM_EDITOR_PASTE", False);
    XSetWMProtocols(a->display, a->window, &a->delete_window, 1);
    a->gc = XCreateGC(a->display, a->window, 0, NULL);
    a->draw = XftDrawCreate(a->display, a->window, a->visual,
                             DefaultColormap(a->display, a->screen));
    a->font = XftFontOpenName(a->display, a->screen, "DejaVu Sans Mono:size=14");
    a->font_small = XftFontOpenName(a->display, a->screen, "DejaVu Sans:size=11");
    a->font_title = XftFontOpenName(a->display, a->screen, "DejaVu Sans:bold:size=18");
    if (!a->draw || !a->font || !a->font_small || !a->font_title) return false;
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
        puts("Heurism Files/Editor 0.5 (C/X11/Xft/GIO)"); return 0;
    }
    setlocale(LC_CTYPE, "");
    struct app a = {.selected = -1, .draft_lock = -1};
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
        a.capacity = 1;
        if (argc == 2) set_file(&a, argv[1]);
        prepare_draft(&a, argc != 2);
    }
    configure_buttons(&a);
    if (!setup(&a)) return fprintf(stderr, "Heurism application requires X11 TrueColor\n"), 1;
    render(&a);
    bool running = true;
    bool window_alive = true;
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
            else if (event.type == MotionNotify && a.mode == EDITOR && a.selecting) {
                editor_cursor_from_point(&a, event.xmotion.x, event.xmotion.y);
                render(&a);
            } else if (event.type == ButtonRelease && a.mode == EDITOR &&
                       event.xbutton.button == Button1 && a.selecting) {
                a.selecting = false;
                editor_cursor_from_point(&a, event.xbutton.x, event.xbutton.y);
                own_selection(&a, XA_PRIMARY, event.xbutton.time);
                render(&a);
            } else if (event.type == SelectionRequest)
                selection_request(&a, &event.xselectionrequest);
            else if (event.type == SelectionNotify &&
                     event.xselection.selection == a.pending_paste) {
                selection_notify(&a, &event.xselection);
                render(&a);
            } else if (event.type == PropertyNotify && a.paste_incr &&
                       event.xproperty.atom == a.paste_property &&
                       event.xproperty.state == PropertyNewValue) {
                paste_property_notify(&a, &event.xproperty);
                render(&a);
            } else if (event.type == SelectionClear) {
                if (event.xselectionclear.selection == XA_PRIMARY) a.owns_primary = false;
                if (event.xselectionclear.selection == a.clipboard) a.owns_clipboard = false;
            }
            else if (event.type == FocusIn && a.input_context) XSetICFocus(a.input_context);
            else if (event.type == FocusOut && a.input_context) XUnsetICFocus(a.input_context);
            else if (event.type == DestroyNotify &&
                     event.xdestroywindow.window == a.window) {
                window_alive = false;
                running = false;
            }
            else if (event.type == ClientMessage &&
                     (Atom)event.xclient.data.l[0] == a.delete_window) running = false;
        }
        time_t now = time(NULL);
        if (a.pending_paste != None && now > a.paste_deadline) {
            cancel_paste(&a, "Paste timed out");
            render(&a);
        }
        if (a.mode == EDITOR && a.dirty && now - a.last_draft >= 2) {
            save_draft(&a);
            a.last_draft = now;
        }
        struct pollfd input = {.fd = ConnectionNumber(a.display), .events = POLLIN};
        int ready = poll(&input, 1, 250);
        if (ready < 0 && errno != EINTR) break;
    }
    if (a.mode == EDITOR && a.dirty) save_draft(&a);
    if (a.draft_lock >= 0) {
        close(a.draft_lock);
        if (access(a.draft, F_OK)) unlink(a.draft_lock_path);
    }
    if (a.input_context) XDestroyIC(a.input_context);
    if (a.input_method) XCloseIM(a.input_method);
    if (window_alive) XftDrawDestroy(a.draw);
    XftFontClose(a.display, a.font);
    XftFontClose(a.display, a.font_small);
    XftFontClose(a.display, a.font_title);
    XFreeGC(a.display, a.gc);
    if (window_alive) XDestroyWindow(a.display, a.window);
    XCloseDisplay(a.display);
    clear_undo(&a);
    free(a.primary_text);
    free(a.clipboard_text);
    free(a.paste_text);
    free(a.text);
    return 0;
}

#define _GNU_SOURCE
/* Verify native release contents and the live graphical session. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ROOT "/opt/heurism/native/releases/"
#define CURRENT "/opt/heurism/native/current"
#define PREVIOUS "/var/lib/companion/native-previous-release"
#define HEALTH "/var/lib/companion/desktop-user/.local/state/heurism/session-health-native.json"

static const char *required[] = {
    "heurism-sh", "heurism-terminal", "heurism-control", "heurismctl",
    "heurism-desktop", "heurism-app", "heurism-files", "heurism-editor",
    "heurism-session-config", "heurism-release", "session.sh", "client.sh",
    "user-session.sh", "xfce-power-panel.sh", "openbox.xml", "heurism-settings.desktop",
    "heurism-terminal.desktop", "heurism-power.desktop",
    "xfce4-power-manager.desktop"
};

static bool safe_release(const char *path, char *resolved, size_t size) {
    char *canonical = realpath(path, NULL);
    if (!canonical) return false;
    size_t prefix = strlen(ROOT);
    bool valid = !strncmp(canonical, ROOT, prefix) && canonical[prefix] &&
                 !strchr(canonical + prefix, '/') && strlen(canonical) < size;
    struct stat info;
    valid = valid && !lstat(canonical, &info) && S_ISDIR(info.st_mode) &&
            info.st_uid == 0 && !(info.st_mode & 0022);
    if (valid) strcpy(resolved, canonical);
    free(canonical);
    return valid;
}

static bool valid_name(const char *name) {
    if (!*name || name[0] == '.' || strlen(name) > NAME_MAX) return false;
    for (const char *p = name; *p; p++)
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_' || *p == '.')) return false;
    return true;
}

static bool verify(const char *path) {
    char release[PATH_MAX];
    if (!safe_release(path, release, sizeof release)) return false;
    char manifest[PATH_MAX];
    if (snprintf(manifest, sizeof manifest, "%s/hashes.sha256", release) >= (int)sizeof manifest)
        return false;
    struct stat info;
    if (lstat(manifest, &info) || !S_ISREG(info.st_mode) || info.st_uid ||
        (info.st_mode & 0022)) return false;
    FILE *file = fopen(manifest, "r");
    if (!file) return false;
    bool found[sizeof required / sizeof required[0]] = {0};
    char line[PATH_MAX + 128];
    int count = 0;
    bool good = true;
    while (fgets(line, sizeof line, file)) {
        size_t length = strlen(line);
        if (length < 68 || line[64] != ' ' || line[65] != ' ' || line[length - 1] != '\n') {
            good = false; break;
        }
        for (int i = 0; i < 64; i++) if (!isxdigit((unsigned char)line[i])) good = false;
        line[length - 1] = 0;
        const char *name = line + 66;
        if (!good || !valid_name(name) || !strcmp(name, "hashes.sha256")) { good = false; break; }
        char item[PATH_MAX];
        if (snprintf(item, sizeof item, "%s/%s", release, name) >= (int)sizeof item ||
            lstat(item, &info) || !S_ISREG(info.st_mode) || info.st_uid ||
            (info.st_mode & 0022)) { good = false; break; }
        for (size_t i = 0; i < sizeof required / sizeof required[0]; i++)
            if (!strcmp(name, required[i])) {
                if (found[i]) good = false;
                found[i] = true;
            }
        count++;
        if (count > 64) { good = false; break; }
    }
    if (ferror(file)) good = false;
    fclose(file);
    for (size_t i = 0; i < sizeof required / sizeof required[0]; i++) if (!found[i]) good = false;
    if (!good) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (!pid) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, 1); dup2(nullfd, 2); close(nullfd); }
        if (chdir(release)) _exit(127);
        execl("/usr/bin/sha256sum", "sha256sum", "-c", "hashes.sha256", (char *)NULL);
        execl("/bin/sha256sum", "sha256sum", "-c", "hashes.sha256", (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool read_text(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) return false;
    if (!fgets(buffer, (int)size, file)) { fclose(file); return false; }
    fclose(file);
    buffer[strcspn(buffer, "\r\n")] = 0;
    return true;
}

static bool healthy(const char *active) {
    char release[PATH_MAX];
    if (!safe_release(active, release, sizeof release)) return false;
    char data[4096];
    if (!read_text(HEALTH, data, sizeof data)) return false;
    struct json_object *record = json_tokener_parse(data), *pid_value = NULL,
                       *uid_value = NULL, *release_value = NULL, *boot_value = NULL,
                       *session_value = NULL;
    if (!record || json_object_get_type(record) != json_type_object ||
        !json_object_object_get_ex(record, "pid", &pid_value) ||
        !json_object_object_get_ex(record, "uid", &uid_value) ||
        !json_object_object_get_ex(record, "release", &release_value) ||
        !json_object_object_get_ex(record, "boot_id", &boot_value)) {
        if (record) json_object_put(record);
        return false;
    }
    struct passwd *account = getpwnam("companion-ui");
    int pid = json_object_get_int(pid_value);
    json_object_object_get_ex(record, "session", &session_value);
    const char *session = session_value ? json_object_get_string(session_value) : "native";
    bool valid = json_object_get_type(pid_value) == json_type_int &&
                 json_object_get_type(uid_value) == json_type_int &&
                 json_object_get_type(release_value) == json_type_string &&
                 json_object_get_type(boot_value) == json_type_string &&
                 account && pid > 1 && pid < 4194304 &&
                 json_object_get_int(uid_value) == (int)account->pw_uid &&
                 !strcmp(json_object_get_string(release_value), release) &&
                 session && (!strcmp(session, "native") || !strcmp(session, "xfce"));
    char boot[64], path[PATH_MAX], executable[PATH_MAX], expected[PATH_MAX];
    valid = valid && read_text("/proc/sys/kernel/random/boot_id", boot, sizeof boot) &&
            !strcmp(json_object_get_string(boot_value), boot) &&
            snprintf(path, sizeof path, "/proc/%d/exe", pid) < (int)sizeof path &&
            (!strcmp(session, "xfce") ?
             snprintf(expected, sizeof expected, "%s", "/usr/bin/xfce4-session") :
             snprintf(expected, sizeof expected, "%s/heurism-desktop", release)) <
                (int)sizeof expected;
    if (valid) {
        ssize_t length = readlink(path, executable, sizeof executable - 1);
        if (length <= 0 || length >= (ssize_t)sizeof executable - 1) valid = false;
        else { executable[length] = 0; valid = !strcmp(executable, expected); }
        snprintf(path, sizeof path, "/proc/%d", pid);
        struct stat info;
        valid = valid && !stat(path, &info) && info.st_uid == account->pw_uid;
    }
    if (valid) puts(data);
    json_object_put(record);
    return valid;
}

static bool rollback(void) {
    char previous[PATH_MAX], current[PATH_MAX];
    if (!read_text(PREVIOUS, previous, sizeof previous) || !verify(previous) ||
        !safe_release(CURRENT, current, sizeof current)) return false;
    const char *temporary = "/opt/heurism/native/current.rollback";
    if (unlink(temporary) && errno != ENOENT) return false;
    if (symlink(previous, temporary) || rename(temporary, CURRENT)) {
        unlink(temporary); return false;
    }
    FILE *file = fopen(PREVIOUS, "w");
    if (!file) return false;
    bool good = fprintf(file, "%s\n", current) > 0 && fflush(file) == 0 &&
                fsync(fileno(file)) == 0;
    if (fclose(file)) good = false;
    return good;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism native release 0.1 (C)"); return 0;
    }
    if (argc < 2 || argc > 3) return fprintf(stderr, "usage: heurism-release verify [path] | health | rollback\n"), 2;
    if (!strcmp(argv[1], "verify") && argc <= 3) {
        bool good = verify(argc == 3 ? argv[2] : CURRENT);
        if (good) puts("Native release verified");
        return good ? 0 : 1;
    }
    if (!strcmp(argv[1], "health") && argc <= 3)
        return healthy(argc == 3 ? argv[2] : CURRENT) ? 0 : 1;
    if (argc == 2 && !strcmp(argv[1], "rollback") && geteuid() == 0)
        return rollback() ? 0 : 1;
    return 2;
}

#define _GNU_SOURCE
/* Heurism's local control API. No network listener or arbitrary command API. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <ifaddrs.h>
#include <json-c/json.h>
#include <math.h>
#include <net/if.h>
#include <openssl/evp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SOCKET "/run/heurism-desktop/control.sock"
#define DEFAULT_STATE "/var/lib/companion/desktop/preferences.json"
#define FRAME_MAX 4096

static volatile sig_atomic_t stopping;
static const char *socket_path = DEFAULT_SOCKET;
static const char *state_path = DEFAULT_STATE;
static struct json_object *preferences;
static bool dell;
static bool save_preferences(void);
static bool number_file(const char *path, long *number);

static void stop_service(int signal_number) { (void)signal_number; stopping = 1; }

static bool write_all(int fd, const char *data, size_t length) {
    while (length) {
        ssize_t n = write(fd, data, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n;
        length -= (size_t)n;
    }
    return true;
}

static bool read_text(const char *path, char *buffer, size_t size) {
    if (!size) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    size_t n = fread(buffer, 1, size - 1, file);
    bool valid = !ferror(file) && (feof(file) || n < size - 1);
    fclose(file);
    if (!valid) return false;
    buffer[n] = 0;
    while (n && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r' || buffer[n - 1] == ' '))
        buffer[--n] = 0;
    return true;
}

static struct json_object *string_file(const char *path) {
    char value[256];
    return read_text(path, value, sizeof value) ? json_object_new_string(value) : NULL;
}

static bool vm_profile(void) {
    struct stat info;
    if (lstat("/etc/companion/platform.json", &info) || !S_ISREG(info.st_mode) ||
        info.st_uid || (info.st_mode & 0022)) return false;
    char config[128], vendor[128], model[128];
    if (!read_text("/etc/companion/platform.json", config, sizeof config) ||
        !read_text("/sys/class/dmi/id/sys_vendor", vendor, sizeof vendor) ||
        !read_text("/sys/class/dmi/id/product_name", model, sizeof model)) return false;
    struct json_object *root = json_tokener_parse(config), *profile = NULL;
    bool valid = root && json_object_get_type(root) == json_type_object &&
                 json_object_object_length(root) == 1 &&
                 json_object_object_get_ex(root, "platform", &profile) &&
                 json_object_get_type(profile) == json_type_string &&
                 !strcmp(json_object_get_string(profile), "hyperv-dev") &&
                 !strcmp(vendor, "Microsoft Corporation") &&
                 !strcmp(model, "Virtual Machine");
    if (root) json_object_put(root);
    return valid;
}

static bool dell_profile(void) {
    char vendor[128], model[128];
    return access("/etc/companion/platform.json", F_OK) != 0 && errno == ENOENT &&
           read_text("/sys/class/dmi/id/sys_vendor", vendor, sizeof vendor) &&
           read_text("/sys/class/dmi/id/product_name", model, sizeof model) &&
           !strcmp(vendor, "Dell Inc.") && !strcmp(model, "Inspiron 7506 2n1");
}

static bool service_running(const char *name) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (!pid) {
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, 1); dup2(nullfd, 2); close(nullfd); }
        execl("/sbin/rc-service", "rc-service", name, "status", (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool command_run(char *const arguments[], char *output, size_t size,
                        int timeout_ms, bool as_desktop) {
    int channels[2];
    if (!size || pipe(channels)) return false;
    pid_t pid = fork();
    if (pid < 0) {
        close(channels[0]); close(channels[1]);
        return false;
    }
    if (!pid) {
        close(channels[0]);
        if (dup2(channels[1], STDOUT_FILENO) < 0) _exit(127);
        close(channels[1]);
        int nullfd = open("/dev/null", O_WRONLY);
        if (nullfd >= 0) { dup2(nullfd, STDERR_FILENO); close(nullfd); }
        const char *authority = access("/opt/heurism/native/current", F_OK) == 0 &&
            access("/run/heurism-desktop/Xauthority", R_OK) == 0 ?
            "/run/heurism-desktop/Xauthority" : "/run/companion-desktop/Xauthority";
        if (setenv("XAUTHORITY", authority, 1)) _exit(127);
        if (as_desktop) {
            struct passwd *user = getpwnam("companion-ui");
            if (!user || setgid(user->pw_gid) || setuid(user->pw_uid) ||
                setenv("HOME", user->pw_dir, 1) ||
                setenv("XAUTHORITY",
                       access("/opt/heurism/native/current", F_OK) == 0 &&
                       access("/run/heurism-desktop/Xauthority", R_OK) == 0 ?
                           "/run/heurism-desktop/Xauthority" :
                           "/run/companion-desktop/Xauthority", 1) ||
                setenv("XDG_RUNTIME_DIR",
                       access("/opt/heurism/native/current", F_OK) == 0 &&
                       access("/run/heurism-desktop/user", F_OK) == 0 ?
                           "/run/heurism-desktop/user" : "/run/companion-desktop/user", 1)) _exit(127);
        }
        execv(arguments[0], arguments);
        _exit(127);
    }
    close(channels[1]);
    size_t used = 0;
    bool bounded = true, finished = false;
    struct timespec started, now;
    clock_gettime(CLOCK_MONOTONIC, &started);
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - started.tv_sec) * 1000 +
                       (now.tv_nsec - started.tv_nsec) / 1000000;
        if (elapsed >= timeout_ms) { bounded = false; break; }
        struct pollfd event = {.fd = channels[0], .events = POLLIN};
        int ready = poll(&event, 1, timeout_ms - (int)elapsed);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) { bounded = false; break; }
        char buffer[512];
        ssize_t count = read(channels[0], buffer, sizeof buffer);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { bounded = false; break; }
        if (!count) { finished = true; break; }
        if (used + (size_t)count >= size) { bounded = false; break; }
        memcpy(output + used, buffer, (size_t)count);
        used += (size_t)count;
    }
    close(channels[0]);
    if (!bounded || !finished) kill(pid, SIGKILL);
    int status = 0;
    for (;;) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) break;
        if (result < 0 && errno != EINTR) return false;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - started.tv_sec) * 1000 +
                       (now.tv_nsec - started.tv_nsec) / 1000000;
        if (elapsed >= timeout_ms) { kill(pid, SIGKILL); waitpid(pid, &status, 0); bounded = false; break; }
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000};
        nanosleep(&pause, NULL);
    }
    output[used] = 0;
    return bounded && finished && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool command_output(char *const arguments[], char *output, size_t size) {
    return command_run(arguments, output, size, 10000, false);
}

static bool verified_vm_boot(void) {
    if (!vm_profile()) return false;
    struct stat manifest, efi;
    if (lstat("/etc/companion/vm-protected.sha256", &manifest) ||
        !S_ISREG(manifest.st_mode) || manifest.st_uid || (manifest.st_mode & 0022) ||
        stat("/sys/firmware/efi", &efi) || !S_ISDIR(efi.st_mode)) return false;
    char current[64], healthy[64], output[8192];
    if (!read_text("/proc/sys/kernel/random/boot_id", current, sizeof current) ||
        !read_text("/var/lib/companion/healthy-boot-id", healthy, sizeof healthy) ||
        strcmp(current, healthy) || !service_running("sshd") ||
        !service_running("companion-watch")) return false;
    char *hashes[] = {"/usr/bin/sha256sum", "-c", "/etc/companion/vm-protected.sha256", NULL};
    if (!command_output(hashes, output, sizeof output)) return false;
    char *firmware[] = {"/usr/sbin/efibootmgr", NULL};
    if (!command_output(firmware, output, sizeof output)) return false;
    return !strstr(output, "BootNext:");
}

static bool line_value(const char *text, const char *prefix, char *value, size_t size) {
    size_t prefix_length = strlen(prefix);
    for (const char *line = text; line && *line; ) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) : strlen(line);
        if (length >= prefix_length && !strncmp(line, prefix, prefix_length)) {
            size_t copy = length - prefix_length;
            if (copy >= size) return false;
            memcpy(value, line + prefix_length, copy);
            value[copy] = 0;
            return true;
        }
        line = end ? end + 1 : NULL;
    }
    return false;
}

static bool boot_entry(const char *text, const char *number, const char *first,
                       const char *second, const char *third) {
    char prefix[16], entry[1024];
    if (snprintf(prefix, sizeof prefix, "Boot%s", number) >= (int)sizeof prefix ||
        !line_value(text, prefix, entry, sizeof entry)) return false;
    return strstr(entry, first) && strstr(entry, second) && strstr(entry, third);
}

static bool nic_entry(const char *text, const char *number) {
    char prefix[16], entry[1024];
    if (snprintf(prefix, sizeof prefix, "Boot%s", number) >= (int)sizeof prefix ||
        !line_value(text, prefix, entry, sizeof entry)) return false;
    return strstr(entry, "MAC(") && strstr(entry, "{auto_created_boot_option}") &&
           (strstr(entry, "IPv4(") || strstr(entry, "IPv6("));
}

static bool verified_dell_boot(bool restore_order) {
    if (!dell_profile()) return false;
    struct stat manifest;
    const char *path = "/var/lib/companion/presentation-20260927T161814Z/protected.sha256";
    if (lstat(path, &manifest) || !S_ISREG(manifest.st_mode) ||
        manifest.st_uid || (manifest.st_mode & 0022)) return false;
    char current[64], healthy[64], output[8192], drivers[8192], order[256];
    if (!read_text("/proc/sys/kernel/random/boot_id", current, sizeof current) ||
        !read_text("/var/lib/companion/healthy-boot-id", healthy, sizeof healthy) ||
        strcmp(current, healthy) || !service_running("sshd") ||
        !service_running("companion-watch")) return false;
    char *hashes[] = {"/usr/bin/sha256sum", "-c", (char *)path, NULL};
    char *driver_command[] = {"/usr/sbin/efibootmgr", "--driver", NULL};
    char *boot_command[] = {"/usr/sbin/efibootmgr", "-v", NULL};
    if (!command_output(hashes, output, sizeof output) ||
        !command_output(driver_command, drivers, sizeof drivers) ||
        !line_value(drivers, "DriverOrder: ", order, sizeof order) ||
        strcmp(order, "0000,0001") ||
        !command_output(boot_command, output, sizeof output) ||
        strstr(output, "BootNext:") ||
        !line_value(output, "BootOrder: ", order, sizeof order) ||
        !(boot_entry(output, "0005", "\\EFI\\alpine\\grubx64.efi", "Companion Management\tHD(", "HD(") ||
          boot_entry(output, "0005", "\\EFI\\alpine\\grubx64.efi", "Heurism Management\tHD(", "HD(")) ||
        !boot_entry(output, "0000", "\\EFI\\Boot\\BootX64.efi", "NVMe", "HD("))
        return false;
    char copy[256];
    snprintf(copy, sizeof copy, "%s", order);
    char *save = NULL, *entry = strtok_r(copy, ",", &save);
    if (!entry || strcmp(entry, "0005") || !(entry = strtok_r(NULL, ",", &save)) ||
        strcmp(entry, "0000")) return false;
    bool extras = false;
    while ((entry = strtok_r(NULL, ",", &save))) {
        if (strlen(entry) != 4 || strspn(entry, "0123456789abcdefABCDEF") != 4 ||
            !nic_entry(output, entry)) return false;
        extras = true;
    }
    if (!extras || !restore_order) return true;
    int fd = open("/run/companion/power-boot-before.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;
    bool saved = write_all(fd, output, strlen(output)) && fsync(fd) == 0;
    if (close(fd)) saved = false;
    if (!saved) return false;
    char *restore[] = {"/usr/sbin/efibootmgr", "-o", "0005,0000", NULL};
    return command_output(restore, drivers, sizeof drivers) &&
           command_output(boot_command, output, sizeof output) &&
           !strstr(output, "BootNext:") &&
           line_value(output, "BootOrder: ", order, sizeof order) &&
           !strcmp(order, "0005,0000");
}

static bool start_detached(const char *operation) {
    pid_t child = fork();
    if (child < 0) return false;
    if (!child) {
        pid_t worker = fork();
        if (worker < 0) _exit(127);
        if (worker) _exit(0);
        setsid();
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd >= 0) {
            dup2(nullfd, STDIN_FILENO);
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            if (nullfd > STDERR_FILENO) close(nullfd);
        }
        sleep(2);
        if (!strcmp(operation, "reboot"))
            execl("/sbin/reboot", "reboot", (char *)NULL);
        else if (!strcmp(operation, "poweroff"))
            execl("/sbin/poweroff", "poweroff", (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static struct json_object *network_addresses(void) {
    struct json_object *result = json_object_new_array();
    struct ifaddrs *list = NULL;
    if (getifaddrs(&list)) return result;
    for (struct ifaddrs *item = list; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET ||
            !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
        char address[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &((struct sockaddr_in *)item->ifa_addr)->sin_addr,
                       address, sizeof address)) continue;
        uint32_t mask = item->ifa_netmask ?
            ntohl(((struct sockaddr_in *)item->ifa_netmask)->sin_addr.s_addr) : 0;
        int prefix = __builtin_popcount(mask);
        struct json_object *entry = json_object_new_object();
        json_object_object_add(entry, "interface", json_object_new_string(item->ifa_name));
        json_object_object_add(entry, "address", json_object_new_string(address));
        json_object_object_add(entry, "prefix", json_object_new_int(prefix));
        json_object_array_add(result, entry);
    }
    freeifaddrs(list);
    return result;
}

static bool dell_wireless(void) {
    struct stat info;
    return dell && !stat("/sys/class/net/wlan0/wireless", &info) && S_ISDIR(info.st_mode);
}

static struct json_object *network_scan(const char **error) {
    if (!dell_wireless()) { *error = "Dell wireless interface unavailable"; return NULL; }
    char output[256];
    char *up[] = {"/sbin/ip", "link", "set", "wlan0", "up", NULL};
    char *scan[] = {"/usr/sbin/iw", "dev", "wlan0", "scan", NULL};
    char *listing = malloc(1024 * 1024);
    if (!listing || !command_run(up, output, sizeof output, 3000, false) ||
        !command_run(scan, listing, 1024 * 1024, 8000, false)) {
        free(listing); *error = "Wireless scan failed"; return NULL;
    }
    struct json_object *result = json_object_new_object();
    struct json_object *networks = json_object_new_array();
    for (char *line = listing; line && *line; ) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        while (*line == ' ' || *line == '\t') line++;
        if (!strncmp(line, "SSID: ", 6)) {
            const char *name = line + 6;
            size_t length = strlen(name);
            bool safe = length >= 1 && length <= 32;
            for (size_t i = 0; safe && i < length; i++)
                if ((unsigned char)name[i] < 32 || (unsigned char)name[i] == 127) safe = false;
            if (safe) {
                bool seen = false;
                for (size_t i = 0; i < json_object_array_length(networks); i++)
                    if (!strcmp(name, json_object_get_string(json_object_array_get_idx(networks, i))))
                        seen = true;
                if (!seen && json_object_array_length(networks) < 80)
                    json_object_array_add(networks, json_object_new_string(name));
            }
        }
        line = end ? end + 1 : NULL;
    }
    free(listing);
    json_object_object_add(result, "interface", json_object_new_string("wlan0"));
    json_object_object_add(result, "networks", networks);
    return result;
}

static bool process_has_arguments(pid_t pid, const char *program, const char *flag,
                                  const char *expected) {
    char path[64], bytes[2048];
    snprintf(path, sizeof path, "/proc/%ld/cmdline", (long)pid);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t count = read(fd, bytes, sizeof bytes);
    close(fd);
    if (count <= 0 || count == (ssize_t)sizeof bytes || bytes[count - 1]) return false;
    const char *first = strrchr(bytes, '/');
    if (strcmp(first ? first + 1 : bytes, program)) return false;
    bool found = false;
    for (size_t offset = strlen(bytes) + 1; offset < (size_t)count; ) {
        const char *item = bytes + offset;
        size_t length = strlen(item);
        if (!strcmp(item, flag) && offset + length + 1 < (size_t)count &&
            !strcmp(item + length + 1, expected)) found = true;
        offset += length + 1;
    }
    return found;
}

static bool wireless_owner(pid_t *owner) {
    *owner = 0;
    glob_t found = {0};
    if (glob("/proc/[0-9]*/cmdline", 0, NULL, &found)) return true;
    bool valid = true;
    for (size_t i = 0; i < found.gl_pathc; i++) {
        const char *start = found.gl_pathv[i] + strlen("/proc/");
        char *end = NULL;
        long number = strtol(start, &end, 10);
        if (number <= 0 || !end || strcmp(end, "/cmdline")) continue;
        pid_t pid = (pid_t)number;
        if (!process_has_arguments(pid, "wpa_supplicant", "-i", "wlan0")) continue;
        if (!process_has_arguments(pid, "wpa_supplicant", "-c", "/etc/companion/wifi.conf") ||
            *owner) { valid = false; break; }
        *owner = pid;
    }
    globfree(&found);
    return valid;
}

static bool wireless_dhcp(pid_t *owner) {
    *owner = 0;
    struct stat entry;
    const char *path = "/run/companion/dhcp-wlan0.pid";
    if (lstat(path, &entry)) return errno == ENOENT;
    if (!S_ISREG(entry.st_mode) || entry.st_uid || (entry.st_mode & 0022)) return false;
    long number;
    if (!number_file(path, &number) || number <= 0 || number > INT32_MAX ||
        !process_has_arguments((pid_t)number, "udhcpc", "-i", "wlan0") ||
        !process_has_arguments((pid_t)number, "udhcpc", "-p", path)) return false;
    *owner = (pid_t)number;
    return true;
}

static bool write_wifi_config(const char *contents, size_t length) {
    char temporary[] = "/etc/companion/wifi.conf.new.XXXXXX";
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    bool good = !fchmod(fd, 0600) && write_all(fd, contents, length) && !fsync(fd);
    if (close(fd)) good = false;
    if (good) good = !rename(temporary, "/etc/companion/wifi.conf");
    if (!good) unlink(temporary);
    return good;
}

static struct json_object *network_status(const char **error) {
    if (!dell_wireless()) { *error = "Dell wireless interface unavailable"; return NULL; }
    pid_t daemon;
    if (!wireless_owner(&daemon)) { *error = "Wireless is managed by another process"; return NULL; }
    char state[64] = "DISCONNECTED", ssid[64] = "", address[INET_ADDRSTRLEN] = "";
    if (daemon) {
        char output[4096];
        char *query[] = {"/sbin/wpa_cli", "-i", "wlan0", "status", NULL};
        if (command_run(query, output, sizeof output, 3000, false)) {
            char *save = NULL;
            for (char *line = strtok_r(output, "\n", &save); line;
                 line = strtok_r(NULL, "\n", &save)) {
                if (!strncmp(line, "wpa_state=", 10)) snprintf(state, sizeof state, "%s", line + 10);
                if (!strncmp(line, "ssid=", 5)) snprintf(ssid, sizeof ssid, "%s", line + 5);
            }
        }
    }
    struct ifaddrs *list = NULL;
    if (!getifaddrs(&list)) {
        for (struct ifaddrs *item = list; item; item = item->ifa_next)
            if (item->ifa_addr && !strcmp(item->ifa_name, "wlan0") &&
                item->ifa_addr->sa_family == AF_INET)
                inet_ntop(AF_INET, &((struct sockaddr_in *)item->ifa_addr)->sin_addr,
                          address, sizeof address);
        freeifaddrs(list);
    }
    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "interface", json_object_new_string("wlan0"));
    json_object_object_add(result, "state", json_object_new_string(state));
    json_object_object_add(result, "ssid", json_object_new_string(ssid));
    json_object_object_add(result, "address", json_object_new_string(address));
    return result;
}

static struct json_object *network_connect(struct json_object *value, const char **error) {
    struct json_object *ssid = NULL, *password = NULL;
    if (!dell_wireless() || !value || json_object_get_type(value) != json_type_object ||
        json_object_object_length(value) != 2 ||
        !json_object_object_get_ex(value, "ssid", &ssid) ||
        !json_object_object_get_ex(value, "password", &password) ||
        json_object_get_type(ssid) != json_type_string ||
        json_object_get_type(password) != json_type_string) {
        *error = "Wi-Fi requires a name and WPA password"; return NULL;
    }
    const char *name = json_object_get_string(ssid), *secret = json_object_get_string(password);
    size_t name_length = (size_t)json_object_get_string_len(ssid);
    size_t secret_length = (size_t)json_object_get_string_len(password);
    if (name_length < 1 || name_length > 32 || strlen(name) != name_length ||
        strchr(name, '\n') || strchr(name, '\r') ||
        secret_length < 8 || secret_length > 63 || strlen(secret) != secret_length ||
        strchr(secret, '\n') || strchr(secret, '\r')) {
        *error = "Invalid Wi-Fi name or WPA password"; return NULL;
    }
    pid_t daemon, dhcp;
    if (!wireless_owner(&daemon) || !wireless_dhcp(&dhcp)) {
        *error = "Wireless is managed by another process"; return NULL;
    }
    char previous[4096];
    bool had_previous = false;
    struct stat old;
    if (!lstat("/etc/companion/wifi.conf", &old)) {
        if (!S_ISREG(old.st_mode) || old.st_uid || (old.st_mode & 0077) ||
            old.st_size < 0 || old.st_size >= (off_t)sizeof previous) {
            *error = "Wireless configuration needs management review"; return NULL;
        }
        int fd = open("/etc/companion/wifi.conf", O_RDONLY | O_NOFOLLOW);
        if (fd < 0) { *error = "Could not read wireless configuration"; return NULL; }
        ssize_t count = read(fd, previous, sizeof previous);
        close(fd);
        if (count != old.st_size) { *error = "Could not read wireless configuration"; return NULL; }
        had_previous = true;
    } else if (errno != ENOENT) {
        *error = "Could not inspect wireless configuration"; return NULL;
    }
    unsigned char key[32];
    if (PKCS5_PBKDF2_HMAC_SHA1(secret, (int)secret_length,
                               (const unsigned char *)name, (int)name_length,
                               4096, sizeof key, key) != 1) {
        *error = "Could not derive WPA key"; return NULL;
    }
    static const char hex[] = "0123456789abcdef";
    char config[256];
    size_t used = (size_t)snprintf(config, sizeof config,
        "ctrl_interface=/run/wpa_supplicant\nnetwork={\n\tssid=");
    for (size_t i = 0; i < name_length; i++) {
        unsigned char byte = (unsigned char)name[i];
        config[used++] = hex[byte >> 4];
        config[used++] = hex[byte & 15];
    }
    used += (size_t)snprintf(config + used, sizeof config - used, "\n\tpsk=");
    for (size_t i = 0; i < sizeof key; i++) {
        config[used++] = hex[key[i] >> 4];
        config[used++] = hex[key[i] & 15];
    }
    explicit_bzero(key, sizeof key);
    used += (size_t)snprintf(config + used, sizeof config - used, "\n}\n");
    if (used >= sizeof config || !write_wifi_config(config, used)) {
        explicit_bzero(config, sizeof config);
        *error = "Could not save wireless configuration"; return NULL;
    }
    explicit_bzero(config, sizeof config);
    char output[512];
    char *up[] = {"/sbin/ip", "link", "set", "wlan0", "up", NULL};
    char *reload[] = {"/sbin/wpa_cli", "-i", "wlan0", "reconfigure", NULL};
    char *start[] = {"/sbin/wpa_supplicant", "-B", "-i", "wlan0", "-c",
                     "/etc/companion/wifi.conf", "-P", "/run/companion/wpa-wlan0.pid", NULL};
    bool connected = command_run(up, output, sizeof output, 3000, false) &&
        (daemon ? (command_run(reload, output, sizeof output, 3000, false) &&
                   strstr(output, "OK")) :
                  command_run(start, output, sizeof output, 5000, false));
    if (connected && dhcp) connected = !kill(dhcp, SIGUSR1);
    if (connected && !dhcp) {
        char *dhcp_start[] = {"/sbin/udhcpc", "-i", "wlan0", "-b", "-t", "5", "-T", "2",
                              "-p", "/run/companion/dhcp-wlan0.pid", NULL};
        connected = command_run(dhcp_start, output, sizeof output, 5000, false);
    }
    if (connected) {
        connected = false;
        struct timespec started, now;
        clock_gettime(CLOCK_MONOTONIC, &started);
        for (;;) {
            const char *status_error = NULL;
            struct json_object *status = network_status(&status_error);
            struct json_object *state = status ? json_object_object_get(status, "state") : NULL;
            struct json_object *address = status ? json_object_object_get(status, "address") : NULL;
            struct json_object *actual_ssid = status ? json_object_object_get(status, "ssid") : NULL;
            if (state && address && actual_ssid &&
                !strcmp(json_object_get_string(state), "COMPLETED") &&
                *json_object_get_string(address) &&
                !strcmp(json_object_get_string(actual_ssid), name)) connected = true;
            if (status) json_object_put(status);
            if (connected) break;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed = (now.tv_sec - started.tv_sec) * 1000 +
                           (now.tv_nsec - started.tv_nsec) / 1000000;
            if (elapsed >= 30000) break;
            sleep(1);
        }
    }
    if (!connected) {
        bool restored = had_previous ? write_wifi_config(previous, (size_t)old.st_size) :
                                       unlink("/etc/companion/wifi.conf") == 0;
        if (!daemon) {
            pid_t started = 0;
            if (wireless_owner(&started) && started) {
                if (kill(started, SIGTERM)) restored = false;
                for (int attempt = 0; attempt < 30; attempt++) {
                    if (kill(started, 0) && errno == ESRCH) break;
                    struct timespec pause = {.tv_sec = 0, .tv_nsec = 100000000};
                    nanosleep(&pause, NULL);
                }
                if (!kill(started, 0) || errno != ESRCH) restored = false;
            }
            long recorded;
            if (number_file("/run/companion/wpa-wlan0.pid", &recorded)) {
                if (recorded <= 0 || (!kill((pid_t)recorded, 0) || errno != ESRCH) ||
                    unlink("/run/companion/wpa-wlan0.pid")) restored = false;
            }
        }
        if (daemon && had_previous)
            restored = command_run(reload, output, sizeof output, 3000, false) &&
                       strstr(output, "OK") && restored;
        explicit_bzero(previous, sizeof previous);
        *error = restored ? "Wi-Fi did not associate and obtain an address; prior settings restored" :
                            "Wi-Fi rollback needs management review; Ethernet remains active";
        return NULL;
    }
    explicit_bzero(previous, sizeof previous);
    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "interface", json_object_new_string("wlan0"));
    json_object_object_add(result, "ssid", json_object_get(ssid));
    json_object_object_add(result, "state", json_object_new_string("CONNECTED"));
    return result;
}

static struct json_object *memory_info(void) {
    struct json_object *result = json_object_new_object();
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return result;
    char line[256], key[64];
    long long kib;
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, "%63[^:]: %lld kB", key, &kib) != 2 || kib < 0) continue;
        if (!strcmp(key, "MemTotal") || !strcmp(key, "MemAvailable"))
            json_object_object_add(result, key, json_object_new_int64(kib * 1024));
    }
    fclose(file);
    return result;
}

static bool number_file(const char *path, long *number) {
    char data[64], *end = NULL;
    if (!read_text(path, data, sizeof data)) return false;
    errno = 0;
    long value = strtol(data, &end, 10);
    if (errno || end == data || *end) return false;
    *number = value;
    return true;
}

static bool child_path(char *path, size_t size, const char *root, const char *name) {
    return snprintf(path, size, "%s/%s", root, name) < (int)size;
}

static const char *backlight_path(char *path, size_t size) {
    glob_t found = {0};
    if (glob("/sys/class/backlight/*", 0, NULL, &found) || found.gl_pathc != 1 ||
        strlen(found.gl_pathv[0]) >= size) { globfree(&found); return NULL; }
    strcpy(path, found.gl_pathv[0]);
    globfree(&found);
    return path;
}

static struct json_object *power_supplies(bool batteries) {
    struct json_object *result = json_object_new_array();
    glob_t found = {0};
    if (glob("/sys/class/power_supply/*", 0, NULL, &found)) return result;
    for (size_t i = 0; i < found.gl_pathc && i < 32; i++) {
        const char *root = found.gl_pathv[i];
        char path[256], kind[64], value[128];
        if (!child_path(path, sizeof path, root, "type") ||
            !read_text(path, kind, sizeof kind)) continue;
        bool battery = !strcmp(kind, "Battery");
        if (battery != batteries || (!battery && strcmp(kind, "Mains") &&
                                     strcmp(kind, "USB") && strcmp(kind, "USB_C"))) continue;
        const char *name = strrchr(root, '/');
        struct json_object *item = json_object_new_object();
        json_object_object_add(item, "name", json_object_new_string(name ? name + 1 : root));
        long number;
        if (battery) {
            child_path(path, sizeof path, root, "capacity");
            json_object_object_add(item, "capacity", number_file(path, &number) ?
                json_object_new_int64(number) : NULL);
            child_path(path, sizeof path, root, "status");
            json_object_object_add(item, "status", json_object_new_string(
                read_text(path, value, sizeof value) ? value : "Unknown"));
        } else {
            child_path(path, sizeof path, root, "online");
            json_object_object_add(item, "online", number_file(path, &number) ?
                json_object_new_int64(number) : NULL);
        }
        json_object_array_add(result, item);
    }
    globfree(&found);
    return result;
}

static struct json_object *thermal_zones(void) {
    struct json_object *result = json_object_new_array();
    glob_t found = {0};
    if (glob("/sys/class/thermal/thermal_zone*", 0, NULL, &found)) return result;
    for (size_t i = 0; i < found.gl_pathc && i < 32; i++) {
        char path[256], name[128];
        long temperature;
        if (!child_path(path, sizeof path, found.gl_pathv[i], "temp") ||
            !number_file(path, &temperature) || temperature < -20000 ||
            temperature > 150000) continue;
        child_path(path, sizeof path, found.gl_pathv[i], "type");
        const char *fallback = strrchr(found.gl_pathv[i], '/');
        if (!read_text(path, name, sizeof name))
            snprintf(name, sizeof name, "%s", fallback ? fallback + 1 : "thermal");
        struct json_object *item = json_object_new_object();
        json_object_object_add(item, "name", json_object_new_string(name));
        json_object_object_add(item, "celsius", json_object_new_double(temperature / 1000.0));
        json_object_array_add(result, item);
    }
    globfree(&found);
    return result;
}

static struct json_object *brightness_value(void) {
    char root[128], path[256];
    long current, maximum;
    if (!dell || !backlight_path(root, sizeof root) ||
        !child_path(path, sizeof path, root, "max_brightness") ||
        !number_file(path, &maximum) || maximum <= 0 ||
        !child_path(path, sizeof path, root, "brightness") ||
        !number_file(path, &current)) return NULL;
    return json_object_new_int((int)((100 * current + maximum / 2) / maximum));
}

static struct json_object *set_brightness(struct json_object *value, const char **error) {
    if (!value || json_object_get_type(value) != json_type_int ||
        json_object_get_int(value) < 5 || json_object_get_int(value) > 100) {
        *error = "Brightness must be an integer from 5 to 100"; return NULL;
    }
    char root[128], path[256];
    long maximum;
    if (!dell || !backlight_path(root, sizeof root) ||
        !child_path(path, sizeof path, root, "max_brightness") ||
        !number_file(path, &maximum) || maximum <= 0 ||
        !child_path(path, sizeof path, root, "brightness")) {
        *error = "No supported backlight"; return NULL;
    }
    long target = (maximum * json_object_get_int(value) + 50) / 100;
    if (target < 1) target = 1;
    char number[64];
    int length = snprintf(number, sizeof number, "%ld\n", target);
    int fd = open(path, O_WRONLY);
    bool good = fd >= 0 && write_all(fd, number, (size_t)length);
    if (fd >= 0 && close(fd)) good = false;
    struct json_object *actual = good ? brightness_value() : NULL;
    if (!actual) { *error = "Could not set backlight"; return NULL; }
    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "brightness", actual);
    return result;
}

static struct json_object *sound_status(const char **error) {
    if (!dell) { *error = "No physical speaker on this VM"; return NULL; }
    char volume[2048], mute[256], sink[512];
    char *volume_cmd[] = {"/usr/bin/pactl", "get-sink-volume", "@DEFAULT_SINK@", NULL};
    char *mute_cmd[] = {"/usr/bin/pactl", "get-sink-mute", "@DEFAULT_SINK@", NULL};
    char *sink_cmd[] = {"/usr/bin/pactl", "get-default-sink", NULL};
    if (!command_run(volume_cmd, volume, sizeof volume, 3000, true) ||
        !command_run(mute_cmd, mute, sizeof mute, 3000, true) ||
        !command_run(sink_cmd, sink, sizeof sink, 3000, true)) {
        *error = "Speaker service unavailable"; return NULL;
    }
    int channels[2];
    char *cursor = volume;
    for (int i = 0; i < 2; i++) {
        char *percent = strchr(cursor, '%');
        if (!percent) { *error = "Speaker volume unavailable"; return NULL; }
        char *begin = percent;
        while (begin > cursor && begin[-1] >= '0' && begin[-1] <= '9') begin--;
        if (begin == percent || percent - begin > 3) {
            *error = "Speaker volume unavailable"; return NULL;
        }
        char saved = *percent;
        *percent = 0;
        long level = strtol(begin, NULL, 10);
        *percent = saved;
        if (level < 0 || level > 150) { *error = "Speaker volume unavailable"; return NULL; }
        channels[i] = (int)level;
        cursor = percent + 1;
    }
    if (
        (strncmp(mute, "Mute: yes", 9) && strncmp(mute, "Mute: no", 8))) {
        *error = "Speaker state unavailable"; return NULL;
    }
    size_t length = strlen(sink);
    while (length && (sink[length - 1] == '\n' || sink[length - 1] == '\r'))
        sink[--length] = 0;
    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "sink", json_object_new_string(sink));
    json_object_object_add(result, "volume", json_object_new_int(channels[0]));
    json_object_object_add(result, "volume_left", json_object_new_int(channels[0]));
    json_object_object_add(result, "volume_right", json_object_new_int(channels[1]));
    json_object_object_add(result, "muted", json_object_new_boolean(!strncmp(mute, "Mute: yes", 9)));
    return result;
}

static struct json_object *sound_settings(struct json_object *value, const char **error) {
    if (!dell || !value || json_object_get_type(value) != json_type_object ||
        json_object_object_length(value) < 1 || json_object_object_length(value) > 2) {
        *error = "Unknown sound setting"; return NULL;
    }
    struct json_object *level = NULL, *mute = NULL, *left = NULL, *right = NULL;
    bool volume_change = json_object_object_get_ex(value, "volume", &level);
    bool mute_change = json_object_object_get_ex(value, "muted", &mute);
    bool stereo_change = json_object_object_get_ex(value, "volume_left", &left) &&
                         json_object_object_get_ex(value, "volume_right", &right);
    if ((volume_change && (json_object_get_type(level) != json_type_int ||
        json_object_get_int(level) < 0 || json_object_get_int(level) > 100)) ||
        (mute_change && json_object_get_type(mute) != json_type_boolean) ||
        (stereo_change && (json_object_get_type(left) != json_type_int ||
            json_object_get_type(right) != json_type_int ||
            json_object_get_int(left) < 0 || json_object_get_int(left) > 100 ||
            json_object_get_int(right) < 0 || json_object_get_int(right) > 100)) ||
        (volume_change + mute_change + stereo_change != 1) ||
        json_object_object_length(value) != (stereo_change ? 2 : 1)) {
        *error = "Invalid sound setting"; return NULL;
    }
    char output[512], number[8], second[8];
    bool applied;
    if (volume_change) {
        snprintf(number, sizeof number, "%d%%", json_object_get_int(level));
        char *args[] = {"/usr/bin/pactl", "set-sink-volume", "@DEFAULT_SINK@", number, NULL};
        applied = command_run(args, output, sizeof output, 3000, true);
    } else if (stereo_change) {
        snprintf(number, sizeof number, "%d%%", json_object_get_int(left));
        snprintf(second, sizeof second, "%d%%", json_object_get_int(right));
        char *args[] = {"/usr/bin/pactl", "set-sink-volume", "@DEFAULT_SINK@", number, second, NULL};
        applied = command_run(args, output, sizeof output, 3000, true);
    } else {
        char *args[] = {"/usr/bin/pactl", "set-sink-mute", "@DEFAULT_SINK@",
                        json_object_get_boolean(mute) ? "1" : "0", NULL};
        applied = command_run(args, output, sizeof output, 3000, true);
    }
    if (!applied) { *error = "Could not apply speaker setting"; return NULL; }
    struct json_object *result = sound_status(error);
    struct json_object *actual = NULL;
    if (result && stereo_change) {
        struct json_object *actual_right = NULL;
        if (json_object_object_get_ex(result, "volume_left", &actual) &&
            json_object_object_get_ex(result, "volume_right", &actual_right) &&
            json_object_get_int(actual) == json_object_get_int(left) &&
            json_object_get_int(actual_right) == json_object_get_int(right)) return result;
    } else if (result &&
        json_object_object_get_ex(result, volume_change ? "volume" : "muted", &actual) &&
        json_object_get_int(actual) == json_object_get_int(volume_change ? level : mute)) return result;
    if (result) json_object_put(result);
    *error = "Speaker setting did not read back";
    return NULL;
}

static bool bios_writable(const char *name) {
    static const char *const names[] = {
        "FnLock", "FnLockMode", "KeyboardIllumination",
        "KbdBacklightTimeoutAc", "KbdBacklightTimeoutBatt"
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (!strcmp(name, names[i])) return true;
    return false;
}

static struct json_object *bios_setting(const char *root, bool writable) {
    static const char *const fields[] = {
        "display_name", "type", "current_value", "default_value", "possible_values"
    };
    struct json_object *item = json_object_new_object();
    char path[512], data[2048];
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        struct json_object *value = NULL;
        if (child_path(path, sizeof path, root, fields[i]) &&
            read_text(path, data, sizeof data)) value = json_object_new_string(data);
        json_object_object_add(item, fields[i], value);
    }
    struct json_object *type = NULL;
    json_object_object_get_ex(item, "type", &type);
    json_object_object_add(item, "writable", json_object_new_boolean(
        writable && type && !strcmp(json_object_get_string(type), "enumeration")));
    return item;
}

static struct json_object *bios_list(const char **error) {
    if (!dell) { *error = "Dell BIOS attributes are unavailable on this VM"; return NULL; }
    glob_t found = {0};
    if (glob("/sys/class/firmware-attributes/dell-wmi-sysman/attributes/*", 0,
             NULL, &found)) { *error = "Dell BIOS attributes unavailable"; return NULL; }
    struct json_object *result = json_object_new_object();
    for (size_t i = 0; i < found.gl_pathc && i < 256; i++) {
        char path[512];
        const char *name = strrchr(found.gl_pathv[i], '/');
        if (!name || !child_path(path, sizeof path, found.gl_pathv[i], "current_value") ||
            access(path, R_OK)) continue;
        name++;
        json_object_object_add(result, name, bios_setting(found.gl_pathv[i], bios_writable(name)));
    }
    globfree(&found);
    return result;
}

static struct json_object *bios_set(struct json_object *value, const char **error) {
    struct json_object *name = NULL, *choice = NULL;
    if (!dell || !value || json_object_get_type(value) != json_type_object ||
        json_object_object_length(value) != 2 ||
        !json_object_object_get_ex(value, "name", &name) ||
        !json_object_object_get_ex(value, "value", &choice) ||
        json_object_get_type(name) != json_type_string ||
        json_object_get_type(choice) != json_type_string ||
        !bios_writable(json_object_get_string(name))) {
        *error = "This BIOS setting is read-only in Heurism"; return NULL;
    }
    const char *label = json_object_get_string(name);
    const char *requested = json_object_get_string(choice);
    char root[512], path[544], possible[2048], previous[256];
    if (snprintf(root, sizeof root,
        "/sys/class/firmware-attributes/dell-wmi-sysman/attributes/%s", label) >= (int)sizeof root ||
        !child_path(path, sizeof path, root, "possible_values") ||
        !read_text(path, possible, sizeof possible)) {
        *error = "BIOS attribute unavailable"; return NULL;
    }
    bool allowed = false;
    char *save = NULL;
    for (char *candidate = strtok_r(possible, ";", &save); candidate;
         candidate = strtok_r(NULL, ";", &save))
        if (!strcmp(candidate, requested)) allowed = true;
    if (!allowed || !*requested) { *error = "Unsupported BIOS enum"; return NULL; }
    if (!child_path(path, sizeof path, root, "current_value") ||
        !read_text(path, previous, sizeof previous)) {
        *error = "BIOS attribute unavailable"; return NULL;
    }
    if (strcmp(previous, requested)) {
        int log = open("/var/lib/companion/bios-changes.jsonl", O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (log < 0) { *error = "Could not record BIOS change"; return NULL; }
        struct json_object *record = json_object_new_object();
        time_t now = time(NULL);
        struct tm utc;
        gmtime_r(&now, &utc);
        char stamp[32];
        strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc);
        json_object_object_add(record, "utc", json_object_new_string(stamp));
        json_object_object_add(record, "name", json_object_new_string(label));
        json_object_object_add(record, "previous", json_object_new_string(previous));
        json_object_object_add(record, "requested", json_object_new_string(requested));
        const char *line = json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN);
        bool logged = write_all(log, line, strlen(line)) && write_all(log, "\n", 1) &&
                      fsync(log) == 0;
        if (close(log)) logged = false;
        json_object_put(record);
        if (!logged) { *error = "Could not record BIOS change"; return NULL; }
        int fd = open(path, O_WRONLY);
        bool written = fd >= 0 && write_all(fd, requested, strlen(requested));
        if (fd >= 0 && close(fd)) written = false;
        if (!written) { *error = "Could not set BIOS attribute"; return NULL; }
    }
    struct json_object *result = bios_setting(root, true);
    json_object_object_add(result, "previous", json_object_new_string(previous));
    json_object_object_add(result, "pending_reboot",
        string_file("/sys/class/firmware-attributes/dell-wmi-sysman/attributes/pending_reboot"));
    return result;
}

static bool input_property(const char *output, const char *name, double *value) {
    size_t length = strlen(name);
    for (const char *line = output; line && *line; ) {
        const char *end = strchr(line, '\n');
        size_t width = end ? (size_t)(end - line) : strlen(line);
        while (width && (*line == '\t' || *line == ' ')) { line++; width--; }
        if (width > length + 1 && !strncmp(line, name, length) &&
            line[length] == ' ' && line[length + 1] == '(') {
            const char *colon = memchr(line, ':', width);
            if (!colon) return false;
            char *after = NULL;
            errno = 0;
            double parsed = strtod(colon + 1, &after);
            if (errno || after == colon + 1 || !isfinite(parsed)) return false;
            *value = parsed;
            return true;
        }
        line = end ? end + 1 : NULL;
    }
    return false;
}

static struct json_object *input_status(void) {
    if (!dell) return NULL;
    char output[8192];
    char *query[] = {"/usr/bin/xinput", "list-props", "touchpad", NULL};
    double tap, scroll, speed, typing;
    if (!command_output(query, output, sizeof output) ||
        !input_property(output, "libinput Tapping Enabled", &tap) ||
        !input_property(output, "libinput Natural Scrolling Enabled", &scroll) ||
        !input_property(output, "libinput Accel Speed", &speed) ||
        !input_property(output, "libinput Disable While Typing Enabled", &typing))
        return NULL;
    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "tap", json_object_new_boolean(tap > 0.5));
    json_object_object_add(result, "natural_scroll", json_object_new_boolean(scroll > 0.5));
    json_object_object_add(result, "speed", json_object_new_double(speed));
    json_object_object_add(result, "typing_rejection", json_object_new_boolean(typing > 0.5));
    return result;
}

static bool apply_input_values(struct json_object *values) {
    static const struct { const char *key, *property; } items[] = {
        {"tap", "libinput Tapping Enabled"},
        {"natural_scroll", "libinput Natural Scrolling Enabled"},
        {"speed", "libinput Accel Speed"}
    };
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        struct json_object *value = NULL;
        if (!json_object_object_get_ex(values, items[i].key, &value)) continue;
        char number[64], output[512];
        if (i < 2) snprintf(number, sizeof number, "%d", json_object_get_boolean(value));
        else snprintf(number, sizeof number, "%.6f", json_object_get_double(value));
        char *command[] = {"/usr/bin/xinput", "set-prop", "touchpad",
                           (char *)items[i].property, number, NULL};
        if (!command_output(command, output, sizeof output)) return false;
    }
    return true;
}

static bool input_matches(struct json_object *actual, struct json_object *requested) {
    if (!actual || !requested) return false;
    json_object_object_foreach(requested, key, item) {
        struct json_object *measured = NULL;
        if (!json_object_object_get_ex(actual, key, &measured) ||
            fabs(json_object_get_double(measured) - json_object_get_double(item)) > 0.001)
            return false;
    }
    return true;
}

static struct json_object *input_settings(struct json_object *value, const char **error) {
    if (!dell || !value || json_object_get_type(value) != json_type_object ||
        json_object_object_length(value) < 1 || json_object_object_length(value) > 3) {
        *error = "Unknown input setting"; return NULL;
    }
    json_object_object_foreach(value, key, item) {
        if ((!strcmp(key, "tap") || !strcmp(key, "natural_scroll")) &&
            json_object_get_type(item) == json_type_boolean) continue;
        if (!strcmp(key, "speed") &&
            (json_object_get_type(item) == json_type_double ||
             json_object_get_type(item) == json_type_int) &&
            isfinite(json_object_get_double(item)) &&
            json_object_get_double(item) >= -1 && json_object_get_double(item) <= 1) continue;
        *error = "Invalid input setting"; return NULL;
    }
    struct json_object *previous = input_status();
    if (!previous) { *error = "The libinput touchpad is not ready"; return NULL; }
    struct json_object *input = NULL;
    json_object_object_get_ex(preferences, "input", &input);
    struct json_object *saved = json_tokener_parse(json_object_to_json_string(input));
    if (!saved) { json_object_put(previous); *error = "Could not copy preferences"; return NULL; }
    {
        json_object_object_foreach(value, key, item)
            json_object_object_add(input, key, json_object_get(item));
    }
    bool applied = apply_input_values(value);
    struct json_object *current = applied ? input_status() : NULL;
    if (!input_matches(current, value)) applied = false;
    if (applied) applied = save_preferences();
    if (!applied) {
        apply_input_values(previous);
        json_object_object_add(preferences, "input", saved);
        if (current) json_object_put(current);
        json_object_put(previous);
        *error = "Input settings could not be applied";
        return NULL;
    }
    json_object_put(saved);
    json_object_put(previous);
    return current;
}

static struct json_object *default_preferences(void) {
    struct json_object *root = json_object_new_object(), *input = json_object_new_object();
    json_object_object_add(root, "theme", json_object_new_string("night"));
    json_object_object_add(input, "tap", json_object_new_boolean(1));
    json_object_object_add(input, "natural_scroll", json_object_new_boolean(1));
    json_object_object_add(input, "speed", json_object_new_double(0));
    json_object_object_add(root, "input", input);
    return root;
}

static void load_preferences(void) {
    preferences = default_preferences();
    char content[4096];
    if (!read_text(state_path, content, sizeof content)) return;
    struct json_object *root = json_tokener_parse(content), *theme = NULL, *input = NULL;
    if (!root || json_object_get_type(root) != json_type_object) goto done;
    if (json_object_object_get_ex(root, "theme", &theme) &&
        json_object_get_type(theme) == json_type_string &&
        (!strcmp(json_object_get_string(theme), "night") ||
         !strcmp(json_object_get_string(theme), "light")))
        json_object_object_add(preferences, "theme", json_object_new_string(json_object_get_string(theme)));
    if (json_object_object_get_ex(root, "input", &input) &&
        json_object_get_type(input) == json_type_object) {
        struct json_object *target = NULL;
        json_object_object_get_ex(preferences, "input", &target);
        const char *names[] = {"tap", "natural_scroll", "speed"};
        for (size_t i = 0; i < 3; i++) {
            struct json_object *value = NULL;
            if (!json_object_object_get_ex(input, names[i], &value)) continue;
            if (i < 2 && json_object_get_type(value) == json_type_boolean)
                json_object_object_add(target, names[i], json_object_new_boolean(json_object_get_boolean(value)));
            if (i == 2 && (json_object_get_type(value) == json_type_double ||
                           json_object_get_type(value) == json_type_int)) {
                double speed = json_object_get_double(value);
                if (speed >= -1 && speed <= 1)
                    json_object_object_add(target, names[i], json_object_new_double(speed));
            }
        }
    }
done:
    if (root) json_object_put(root);
}

static bool save_preferences(void) {
    char temporary[512];
    if (snprintf(temporary, sizeof temporary, "%s.new.XXXXXX", state_path) >= (int)sizeof temporary)
        return false;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    if (fchmod(fd, 0600)) { close(fd); unlink(temporary); return false; }
    const char *data = json_object_to_json_string_ext(preferences, JSON_C_TO_STRING_PLAIN);
    size_t length = strlen(data);
    bool good = write_all(fd, data, length) && write_all(fd, "\n", 1) && fsync(fd) == 0;
    if (close(fd)) good = false;
    if (good) good = rename(temporary, state_path) == 0;
    if (!good) unlink(temporary);
    return good;
}

static bool prepare_default_state(void) {
    if (strcmp(state_path, DEFAULT_STATE)) return true;
    if (mkdir("/var/lib/companion/desktop", 0700) && errno != EEXIST) return false;
    struct stat info;
    return !lstat("/var/lib/companion/desktop", &info) &&
           S_ISDIR(info.st_mode) && info.st_uid == 0 && !(info.st_mode & 0022);
}

static struct json_object *status_data(void) {
    struct json_object *root = json_object_new_object();
    char value[256], current[64], healthy[64];
    struct utsname system;
    uname(&system);
    gethostname(value, sizeof value);
    value[sizeof value - 1] = 0;
    json_object_object_add(root, "version", json_object_new_int(1));
    json_object_object_add(root, "platform", json_object_new_string(dell ? "dell" : "hyperv-dev"));
    json_object_object_add(root, "hostname", json_object_new_string(value));
    json_object_object_add(root, "kernel", json_object_new_string(system.release));
    json_object_object_add(root, "boot_id", string_file("/proc/sys/kernel/random/boot_id"));
    if (read_text("/proc/uptime", value, sizeof value))
        json_object_object_add(root, "uptime_seconds", json_object_new_double(strtod(value, NULL)));
    json_object_object_add(root, "memory", memory_info());
    json_object_object_add(root, "network", network_addresses());
    json_object_object_add(root, "batteries", dell ? power_supplies(true) : json_object_new_array());
    json_object_object_add(root, "power", dell ? power_supplies(false) : json_object_new_array());
    json_object_object_add(root, "thermal", dell ? thermal_zones() : json_object_new_array());
    json_object_object_add(root, "brightness", brightness_value());
    struct json_object *management = json_object_new_object();
    json_object_object_add(management, "ssh", json_object_new_boolean(service_running("sshd")));
    json_object_object_add(management, "watch", json_object_new_boolean(service_running("companion-watch")));
    bool boot = read_text("/proc/sys/kernel/random/boot_id", current, sizeof current) &&
                read_text("/var/lib/companion/healthy-boot-id", healthy, sizeof healthy) &&
                !strcmp(current, healthy);
    json_object_object_add(management, "boot_healthy", json_object_new_boolean(boot));
    json_object_object_add(root, "management", management);
    struct json_object *bios = json_object_new_object();
    json_object_object_add(bios, "vendor", string_file("/sys/class/dmi/id/bios_vendor"));
    json_object_object_add(bios, "version", string_file("/sys/class/dmi/id/bios_version"));
    json_object_object_add(bios, "model", string_file("/sys/class/dmi/id/product_name"));
    json_object_object_add(root, "bios", bios);
    json_object_object_add(root, "preferences", json_object_get(preferences));
    return root;
}

static struct json_object *dispatch(struct json_object *request, const char **error) {
    struct json_object *version = NULL, *action = NULL, *value = NULL;
    if (!request || json_object_get_type(request) != json_type_object ||
        !json_object_object_get_ex(request, "version", &version) ||
        json_object_get_type(version) != json_type_int || json_object_get_int(version) != 1 ||
        !json_object_object_get_ex(request, "action", &action) ||
        json_object_get_type(action) != json_type_string) {
        *error = "Unsupported request"; return NULL;
    }
    const char *name = json_object_get_string(action);
    json_object_object_get_ex(request, "value", &value);
    if (!strcmp(name, "status")) return status_data();
    if (!strcmp(name, "theme")) {
        if (!value || json_object_get_type(value) != json_type_string ||
            (strcmp(json_object_get_string(value), "night") &&
             strcmp(json_object_get_string(value), "light"))) {
            *error = "Unsupported theme"; return NULL;
        }
        struct json_object *previous = NULL;
        json_object_object_get_ex(preferences, "theme", &previous);
        json_object_get(previous);
        json_object_object_add(preferences, "theme", json_object_new_string(json_object_get_string(value)));
        if (!save_preferences()) {
            json_object_object_add(preferences, "theme", previous);
            *error = "Could not save preferences"; return NULL;
        }
        json_object_put(previous);
        return json_object_get(preferences);
    }
    if (!strcmp(name, "input-status")) {
        struct json_object *result = input_status();
        if (!result) *error = "The libinput touchpad is unavailable";
        return result;
    }
    if (!strcmp(name, "input-settings")) return input_settings(value, error);
    if (!strcmp(name, "input-apply-saved")) {
        struct json_object *saved = NULL;
        json_object_object_get_ex(preferences, "input", &saved);
        if (!dell) { *error = "The libinput touchpad is unavailable"; return NULL; }
        if (!apply_input_values(saved)) { *error = "Saved input settings could not be applied"; return NULL; }
        struct json_object *result = input_status();
        if (!input_matches(result, saved)) {
            if (result) json_object_put(result);
            *error = "Saved input settings did not read back";
            return NULL;
        }
        return result;
    }
    if (!strcmp(name, "sound-status")) return sound_status(error);
    if (!strcmp(name, "sound-settings")) return sound_settings(value, error);
    if (!strcmp(name, "network-status")) return network_status(error);
    if (!strcmp(name, "network-scan")) return network_scan(error);
    if (!strcmp(name, "network-connect")) return network_connect(value, error);
    else if (!strcmp(name, "bios-list")) return bios_list(error);
    else if (!strcmp(name, "bios-set")) return bios_set(value, error);
    else if (!strcmp(name, "brightness")) return set_brightness(value, error);
    else if (!strcmp(name, "power-check")) {
        if (!(dell ? verified_dell_boot(false) : verified_vm_boot()))
            *error = "Boot and management checks failed";
        else return json_object_new_boolean(1);
    }
    else if (!strcmp(name, "admin-console"))
        *error = "Use authenticated root SSH for administration";
    else if (!strcmp(name, "power")) {
        struct json_object *operation = NULL, *confirm = NULL;
        if (!value || json_object_get_type(value) != json_type_object ||
            json_object_object_length(value) != 2 ||
            !json_object_object_get_ex(value, "operation", &operation) ||
            !json_object_object_get_ex(value, "confirm", &confirm) ||
            json_object_get_type(operation) != json_type_string ||
            json_object_get_type(confirm) != json_type_boolean ||
            !json_object_get_boolean(confirm) ||
            (strcmp(json_object_get_string(operation), "reboot") &&
             strcmp(json_object_get_string(operation), "poweroff")))
            *error = "Confirm the specific power action";
        else if (!(dell ? verified_dell_boot(true) : verified_vm_boot()))
            *error = "Boot and management checks failed";
        else if (!start_detached(json_object_get_string(operation)))
            *error = "Could not schedule power action";
        else {
            struct json_object *result = json_object_new_object();
            json_object_object_add(result, "scheduled",
                json_object_new_string(json_object_get_string(operation)));
            return result;
        }
    }
    else *error = "Unknown action";
    return NULL;
}

static void reply(int client, bool okay, struct json_object *data, const char *error) {
    struct json_object *response = json_object_new_object();
    json_object_object_add(response, "ok", json_object_new_boolean(okay));
    if (okay) json_object_object_add(response, "data", data);
    else json_object_object_add(response, "error", json_object_new_string(error));
    const char *text = json_object_to_json_string_ext(response, JSON_C_TO_STRING_PLAIN);
    size_t length = strlen(text);
    if (length < 65535) { write_all(client, text, length); write_all(client, "\n", 1); }
    json_object_put(response);
}

static void serve_client(int client, uid_t desktop_uid) {
    struct ucred peer;
    socklen_t length = sizeof peer;
    if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &peer, &length) ||
        (peer.uid != 0 && peer.uid != desktop_uid)) {
        reply(client, false, NULL, "Unauthorized local caller"); return;
    }
    char frame[FRAME_MAX + 2];
    size_t used = 0;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (used <= FRAME_MAX) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                       (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= 3000) break;
        struct pollfd input = {.fd = client, .events = POLLIN};
        int ready = poll(&input, 1, (int)(3000 - elapsed));
        if (ready <= 0) break;
        ssize_t n = read(client, frame + used, sizeof frame - used);
        if (n <= 0) break;
        const char *newline = memchr(frame + used, '\n', (size_t)n);
        used += newline ? (size_t)(newline - (frame + used)) + 1 : (size_t)n;
        if (newline) break;
    }
    if (!used || used > FRAME_MAX || frame[used - 1] != '\n') {
        reply(client, false, NULL, "Invalid request frame"); return;
    }
    frame[used - 1] = 0;
    struct json_tokener *parser = json_tokener_new();
    struct json_object *request = json_tokener_parse_ex(parser, frame, (int)used - 1);
    bool valid = json_tokener_get_error(parser) == json_tokener_success &&
                 json_tokener_get_parse_end(parser) == used - 1;
    json_tokener_free(parser);
    const char *error = NULL;
    struct json_object *data = valid ? dispatch(request, &error) : NULL;
    if (!valid) error = "Invalid JSON request";
    reply(client, !error, data, error);
    if (request) json_object_put(request);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism control 0.1 (C)"); return 0;
    }
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) return fprintf(stderr, "usage: heurism-control [--socket path] [--state path]\n"), 2;
        if (!strcmp(argv[i], "--socket")) socket_path = argv[i + 1];
        else if (!strcmp(argv[i], "--state")) state_path = argv[i + 1];
        else return fprintf(stderr, "unknown option: %s\n", argv[i]), 2;
    }
    dell = dell_profile();
    if (geteuid() || !(dell || vm_profile()))
        return fprintf(stderr, "control requires root on a verified Heurism platform\n"), 1;
    struct passwd *desktop = getpwnam("companion-ui");
    if (!desktop) return fprintf(stderr, "companion-ui account missing\n"), 1;
    gid_t desktop_gid = desktop->pw_gid;
    uid_t desktop_uid = desktop->pw_uid;
    if (!prepare_default_state())
        return fprintf(stderr, "control state directory unavailable\n"), 1;
    load_preferences();
    setenv("DISPLAY", ":0", 1);
    setenv("XAUTHORITY",
           access("/opt/heurism/native/current", F_OK) == 0 &&
           access("/run/heurism-desktop/Xauthority", R_OK) == 0 ?
               "/run/heurism-desktop/Xauthority" : "/run/companion-desktop/Xauthority", 1);
    if (!strcmp(socket_path, DEFAULT_SOCKET)) {
        if (mkdir("/run/heurism-desktop", 0750) && errno != EEXIST) return perror("mkdir"), 1;
        if (chown("/run/heurism-desktop", 0, desktop_gid) ||
            chmod("/run/heurism-desktop", 0750)) return perror("socket directory"), 1;
    }
    if (unlink(socket_path) && errno != ENOENT) return perror("socket unlink"), 1;
    int server = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server < 0) return perror("socket"), 1;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (strlen(socket_path) >= sizeof address.sun_path) return fprintf(stderr, "socket path too long\n"), 1;
    strcpy(address.sun_path, socket_path);
    if (bind(server, (struct sockaddr *)&address, sizeof address) ||
        chown(socket_path, 0, desktop_gid) || chmod(socket_path, 0660) ||
        listen(server, 16)) return perror("control socket"), 1;
    struct sigaction action = {.sa_handler = stop_service};
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    puts("Heurism C control ready"); fflush(stdout);
    while (!stopping) {
        int client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
        if (client < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        serve_client(client, desktop_uid);
        close(client);
    }
    close(server);
    unlink(socket_path);
    json_object_put(preferences);
    return 0;
}

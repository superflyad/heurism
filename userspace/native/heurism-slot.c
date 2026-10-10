#define _GNU_SOURCE
/* VM-only A/B boot control. GRUB's one-time request is stored on root A. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MOUNT_DIR "/run/heurism-slot/inactive"
#define ENV_REL "/boot/grub/grubenv"

static const char *protected_paths[] = {
    "/boot/vmlinuz-lts", "/boot/initramfs-lts", "/sbin/init", "/boot/grub/grub.cfg",
    "/boot/efi/EFI/BOOT/BOOTX64.EFI", "/etc/fstab", "/etc/heurism/slot",
    "/etc/network/interfaces", "/etc/ssh/sshd_config",
    "/etc/ssh/ssh_host_ed25519_key.pub", "/etc/init.d/sshd",
    "/etc/init.d/companion-watch", "/etc/init.d/heurism-control",
    "/etc/init.d/heurism-desktop", "/usr/local/sbin/companion-vm-watch",
    "/usr/share/heurism/wallpaper.svg", "/etc/companion/platform.json",
    "/etc/heurism/desktop-lock", "/etc/os-release",
    "/etc/heurism/upstream-release", "/etc/heurism/packages.installed",
    "/etc/heurism/build-provenance"
};

static bool line_is(const char *path, const char *expected) {
    char line[256];
    FILE *file = fopen(path, "r");
    if (!file) return false;
    bool good = fgets(line, sizeof line, file) &&
                line[strcspn(line, "\r\n")] == '\n' && fgetc(file) == EOF;
    if (good) line[strcspn(line, "\r\n")] = 0;
    good = good && !strcmp(line, expected);
    fclose(file);
    return good;
}

static bool run(const char *root, char *const argv[]) {
    pid_t child = fork();
    if (child < 0) return false;
    if (!child) {
        int nullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (nullfd >= 0) {
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
        if (root && (chroot(root) || chdir("/"))) _exit(127);
        execv(argv[0], argv);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool volume_is(const char *path, const char *device) {
    struct stat mounted, block;
    return !stat(path, &mounted) && !stat(device, &block) &&
           S_ISBLK(block.st_mode) && mounted.st_dev == block.st_rdev;
}

static bool guard(char *slot) {
    if (geteuid() || !line_is("/sys/class/dmi/id/sys_vendor", "Microsoft Corporation") ||
        !line_is("/sys/class/dmi/id/product_name", "Virtual Machine") ||
        !line_is("/etc/companion/platform.json", "{\"platform\":\"hyperv-dev\"}"))
        return false;
    if (line_is("/etc/heurism/slot", "A")) *slot = 'A';
    else if (line_is("/etc/heurism/slot", "B")) *slot = 'B';
    else return false;
    return volume_is("/", *slot == 'A' ? "/dev/sda2" : "/dev/sda3") &&
           volume_is("/boot/efi", "/dev/sda1") &&
           volume_is("/var/lib/companion", "/dev/sda4");
}

static bool manifest_shape(const char *root) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s/etc/companion/vm-protected.sha256", root) >=
        (int)sizeof path) return false;
    struct stat info;
    if (lstat(path, &info) || !S_ISREG(info.st_mode) || info.st_uid ||
        (info.st_mode & 0022)) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    bool good = true;
    char line[PATH_MAX + 100];
    size_t count = 0;
    while (fgets(line, sizeof line, file)) {
        size_t length = strlen(line);
        if (count >= sizeof protected_paths / sizeof protected_paths[0] ||
            length < 68 || line[length - 1] != '\n' ||
            line[64] != ' ' || line[65] != ' ') {
            good = false;
            break;
        }
        line[length - 1] = 0;
        if (strcmp(line + 66, protected_paths[count])) { good = false; break; }
        count++;
    }
    if (ferror(file) || count != sizeof protected_paths / sizeof protected_paths[0]) good = false;
    fclose(file);
    return good;
}

static bool boot_services(const char *root) {
    static const char *services[] = {
        "networking", "sshd", "dbus", "companion-watch",
        "heurism-control", "heurism-desktop"
    };
    for (size_t i = 0; i < sizeof services / sizeof services[0]; i++) {
        char path[PATH_MAX], expected[PATH_MAX], actual[PATH_MAX];
        if (snprintf(path, sizeof path, "%s/etc/runlevels/default/%s", root, services[i]) >=
                (int)sizeof path ||
            snprintf(expected, sizeof expected, "/etc/init.d/%s", services[i]) >=
                (int)sizeof expected) return false;
        ssize_t length = readlink(path, actual, sizeof actual - 1);
        if (length <= 0 || length >= (ssize_t)sizeof actual - 1) return false;
        actual[length] = 0;
        if (strcmp(actual, expected)) return false;
    }
    return true;
}

static bool make_mount_dir(void) {
    if (mkdir("/run/heurism-slot", 0700) && errno != EEXIST) return false;
    if (mkdir(MOUNT_DIR, 0700) && errno != EEXIST) return false;
    struct stat info;
    return !lstat("/run/heurism-slot", &info) && S_ISDIR(info.st_mode) &&
           !info.st_uid && !(info.st_mode & 0077) &&
           !lstat(MOUNT_DIR, &info) && S_ISDIR(info.st_mode) &&
           !info.st_uid && !(info.st_mode & 0077);
}

static bool mount_inactive(char slot, bool writable) {
    if (!make_mount_dir()) return false;
    struct stat parent, child;
    if (stat("/run/heurism-slot", &parent) || stat(MOUNT_DIR, &child) ||
        parent.st_dev != child.st_dev) return false;
    const char *device = slot == 'A' ? "/dev/sda2" : "/dev/sda3";
    unsigned long flags = MS_NOSUID | MS_NODEV | (writable ? 0UL : MS_RDONLY);
    if (mount(device, MOUNT_DIR, "ext4", flags, NULL)) return false;
    if (volume_is(MOUNT_DIR, device)) return true;
    umount2(MOUNT_DIR, 0);
    return false;
}

static bool verify_inactive(char slot) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s/etc/heurism/slot", MOUNT_DIR) >=
        (int)sizeof path || !line_is(path, slot == 'A' ? "A" : "B") ||
        !manifest_shape(MOUNT_DIR) || !boot_services(MOUNT_DIR)) return false;
    if (snprintf(path, sizeof path, "%s/boot/efi", MOUNT_DIR) >= (int)sizeof path ||
        mount("/boot/efi", path, NULL, MS_BIND, NULL)) return false;
    bool good = !mount(NULL, path, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
    char *hashes[] = {"/usr/bin/sha256sum", "-c", "/etc/companion/vm-protected.sha256", NULL};
    char *release[] = {"/opt/heurism/native/current/heurism-release", "verify", NULL};
    good = good && run(MOUNT_DIR, hashes) && run(MOUNT_DIR, release);
    if (umount2(path, 0)) good = false;
    return good;
}

static bool healthy(void) {
    char boot[128], marked[128];
    FILE *uptime = fopen("/proc/uptime", "r");
    double seconds = 0;
    if (!uptime || fscanf(uptime, "%lf", &seconds) != 1) {
        if (uptime) fclose(uptime);
        return false;
    }
    fclose(uptime);
    if (seconds < 45 || !line_is("/etc/heurism/slot", "B")) return false;
    FILE *file = fopen("/proc/sys/kernel/random/boot_id", "r");
    if (!file || !fgets(boot, sizeof boot, file)) {
        if (file) fclose(file);
        return false;
    }
    fclose(file);
    file = fopen("/var/lib/companion/healthy-boot-id", "r");
    if (!file || !fgets(marked, sizeof marked, file)) {
        if (file) fclose(file);
        return false;
    }
    fclose(file);
    if (strcmp(boot, marked)) return false;
    char *hashes[] = {"/usr/bin/sha256sum", "-c", "/etc/companion/vm-protected.sha256", NULL};
    char *verify[] = {"/opt/heurism/native/current/heurism-release", "verify", NULL};
    char *session[] = {"/opt/heurism/native/current/heurism-release", "health", NULL};
    char *control[] = {"/opt/heurism/native/current/heurismctl", "status", NULL};
    char *ssh[] = {"/sbin/rc-service", "sshd", "status", NULL};
    char *watch[] = {"/sbin/rc-service", "companion-watch", "status", NULL};
    return run(NULL, hashes) && run(NULL, verify) && run(NULL, session) &&
           run(NULL, control) && run(NULL, ssh) && run(NULL, watch);
}

static bool current_verified(void) {
    char *hashes[] = {"/usr/bin/sha256sum", "-c", "/etc/companion/vm-protected.sha256", NULL};
    char *verify[] = {"/opt/heurism/native/current/heurism-release", "verify", NULL};
    char *session[] = {"/opt/heurism/native/current/heurism-release", "health", NULL};
    char *control[] = {"/opt/heurism/native/current/heurismctl", "status", NULL};
    return run(NULL, hashes) && run(NULL, verify) && run(NULL, session) && run(NULL, control);
}

static bool next_is_one(const char *root) {
    char path[PATH_MAX], data[2048];
    if (snprintf(path, sizeof path, "%s%s", root, ENV_REL) >= (int)sizeof path)
        return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    size_t size = fread(data, 1, sizeof data - 1, file);
    bool good = !ferror(file);
    fclose(file);
    data[size] = 0;
    return good && strstr(data, "next_entry=1\n") != NULL;
}

static bool edit_next(const char *root, bool set) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s%s", root, ENV_REL) >= (int)sizeof path)
        return false;
    char *set_args[] = {"/usr/bin/grub-editenv", path, "set", "next_entry=1", NULL};
    char *unset_args[] = {"/usr/bin/grub-editenv", path, "unset", "next_entry", NULL};
    if (!run(NULL, set ? set_args : unset_args)) return false;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    bool good = fsync(fd) == 0 && syncfs(fd) == 0;
    close(fd);
    return good && (next_is_one(root) == set);
}

int main(int argc, char **argv) {
    if (argc != 2) return fprintf(stderr, "usage: heurism-slot status|verify-inactive|queue|renew|fallback\n"), 2;
    char slot = 0;
    if (!guard(&slot)) return fprintf(stderr, "VM slot or mount guard failed\n"), 1;
    if (!make_mount_dir()) return fprintf(stderr, "slot lock directory failed\n"), 1;
    int lock = open("/run/heurism-slot/lock", O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB))
        return fprintf(stderr, "slot operation already running\n"), 1;
    bool queue = !strcmp(argv[1], "queue");
    bool renew = !strcmp(argv[1], "renew");
    bool fallback = !strcmp(argv[1], "fallback");
    bool verify = !strcmp(argv[1], "verify-inactive");
    bool status = !strcmp(argv[1], "status");
    if (!queue && !renew && !fallback && !verify && !status) return 2;
    if (queue && slot != 'A') return fprintf(stderr, "queue requires A\n"), 1;
    if (renew && slot != 'B') return fprintf(stderr, "renew requires B\n"), 1;
    if (renew && !healthy()) return fprintf(stderr, "B health gate failed\n"), 1;
    if (queue && !current_verified()) return fprintf(stderr, "A health gate failed\n"), 1;
    if (fallback && slot == 'A') {
        char *hashes[] = {"/usr/bin/sha256sum", "-c", "/etc/companion/vm-protected.sha256", NULL};
        bool good = run(NULL, hashes) && edit_next("", false);
        if (good) puts("next boot set to A");
        return good ? 0 : 1;
    }
    char inactive = slot == 'A' ? 'B' : 'A';
    if (!mount_inactive(inactive, renew || fallback))
        return fprintf(stderr, "inactive mount failed\n"), 1;
    bool good = verify_inactive(inactive);
    if (good && queue) good = edit_next("", true);
    else if (good && renew) good = edit_next(MOUNT_DIR, true);
    else if (good && fallback) good = edit_next(slot == 'A' ? "" : MOUNT_DIR, false);
    if (good && status) {
        printf("slot=%c next=%s\n", slot,
               next_is_one(slot == 'A' ? "" : MOUNT_DIR) ? "B" : "A");
    }
    if (umount2(MOUNT_DIR, 0)) good = false;
    if (!good) return fprintf(stderr, "slot operation failed\n"), 1;
    if (queue) puts("B queued for one boot");
    else if (renew) puts("healthy B renewed for one boot");
    else if (fallback) puts("next boot set to A");
    else if (verify) printf("inactive %c verified\n", inactive);
    return 0;
}

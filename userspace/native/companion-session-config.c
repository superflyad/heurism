#define _GNU_SOURCE
/* Generate explicit Xorg devices without touching network drivers. */
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <json-c/json.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static bool read_text(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) return false;
    if (!fgets(buffer, (int)size, file)) { fclose(file); return false; }
    fclose(file);
    buffer[strcspn(buffer, "\r\n")] = 0;
    return true;
}

static bool vm_profile(void) {
    struct stat info;
    if (lstat("/etc/companion/platform.json", &info) || !S_ISREG(info.st_mode) ||
        info.st_uid || (info.st_mode & 0022)) return false;
    char data[128], vendor[128], model[128];
    if (!read_text("/etc/companion/platform.json", data, sizeof data) ||
        !read_text("/sys/class/dmi/id/sys_vendor", vendor, sizeof vendor) ||
        !read_text("/sys/class/dmi/id/product_name", model, sizeof model)) return false;
    struct json_object *root = json_tokener_parse(data), *value = NULL;
    bool valid = root && json_object_get_type(root) == json_type_object &&
                 json_object_object_length(root) == 1 &&
                 json_object_object_get_ex(root, "platform", &value) &&
                 json_object_get_type(value) == json_type_string &&
                 !strcmp(json_object_get_string(value), "hyperv-dev") &&
                 !strcmp(vendor, "Microsoft Corporation") && !strcmp(model, "Virtual Machine");
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

static bool classify(const char *event, const char *label) {
    char sysfs[128], log[128];
    if (snprintf(sysfs, sizeof sysfs, "/sys/class/input/%s", event) >= (int)sizeof sysfs ||
        snprintf(log, sizeof log, "/run/companion-desktop/%s-udev.log", label) >= (int)sizeof log)
        return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (!pid) {
        FILE *output = fopen(log, "w");
        if (output) { dup2(fileno(output), 1); dup2(fileno(output), 2); fclose(output); }
        execl("/sbin/udevadm", "udevadm", "test", sysfs, (char *)NULL);
        execl("/usr/bin/udevadm", "udevadm", "test", sysfs, (char *)NULL);
        _exit(127);
    }
    int status;
    for (int attempt = 0; attempt < 50; attempt++) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if (result < 0 && errno != EINTR) return false;
        struct timespec pause = {.tv_nsec = 100000000};
        nanosleep(&pause, NULL);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return false;
}

static bool classify_audio(void) {
    if (access("/sys/class/sound/card0/controlC0", F_OK)) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (!pid) {
        int log = open("/run/companion-desktop/audio-card0-udev.log", O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (log >= 0) { dup2(log, 1); dup2(log, 2); close(log); }
        execl("/sbin/udevadm", "udevadm", "test", "--action=change",
              "/sys/class/sound/card0", (char *)NULL);
        _exit(127);
    }
    int status;
    for (int attempt = 0; attempt < 50; attempt++) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if (result < 0 && errno != EINTR) return false;
        struct timespec pause = {.tv_nsec = 100000000};
        nanosleep(&pause, NULL);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return false;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Companion session config 0.1 (C)"); return 0;
    }
    const char *output = "/run/companion-desktop/xorg.conf";
    if (argc == 3 && !strcmp(argv[1], "--output")) output = argv[2];
    else if (argc != 1) return fprintf(stderr, "usage: companion-session-config [--output path]\n"), 2;
    bool vm = vm_profile(), dell = dell_profile();
    if (geteuid() || !(vm || dell))
        return fprintf(stderr, "verified Companion root and platform required\n"), 1;
    struct stat info;
    if (vm && (stat("/dev/fb0", &info) || !S_ISCHR(info.st_mode)))
        return fprintf(stderr, "Hyper-V framebuffer missing\n"), 1;
    char display[32] = "", display_path[64] = "";
    char keyboard[32] = "", pointer[32] = "", touchscreen[32] = "";
    if (dell) {
        glob_t cards = {0};
        if (glob("/sys/class/drm/card*/device/vendor", 0, NULL, &cards)) return 1;
        int found = 0;
        for (size_t i = 0; i < cards.gl_pathc; i++) {
            char vendor[32];
            if (!read_text(cards.gl_pathv[i], vendor, sizeof vendor) || strcmp(vendor, "0x8086")) continue;
            const char *card = strstr(cards.gl_pathv[i], "/card");
            if (!card || sscanf(card + 1, "%31[^/]", display) != 1) continue;
            found++;
        }
        globfree(&cards);
        if (found != 1 || snprintf(display_path, sizeof display_path, "/dev/dri/%s", display) >= (int)sizeof display_path ||
            stat(display_path, &info) || !S_ISCHR(info.st_mode))
            return fprintf(stderr, "Expected Intel display unavailable\n"), 1;
        if (!classify_audio()) return fprintf(stderr, "Dell audio classification failed\n"), 1;
    }
    glob_t entries = {0};
    if (glob("/sys/class/input/event*", 0, NULL, &entries))
        return fprintf(stderr, "Input inventory unavailable\n"), 1;
    for (size_t i = 0; i < entries.gl_pathc; i++) {
        const char *event = strrchr(entries.gl_pathv[i], '/');
        if (!event) continue;
        event++;
        char path[256], name[128];
        if (snprintf(path, sizeof path, "%s/device/name", entries.gl_pathv[i]) >= (int)sizeof path ||
            !read_text(path, name, sizeof name)) continue;
        if (!strcmp(name, "AT Translated Set 2 keyboard"))
            snprintf(keyboard, sizeof keyboard, "%s", event);
        else if (vm && !strcmp(name, "Microsoft Vmbus HID-compliant Mouse"))
            snprintf(pointer, sizeof pointer, "%s", event);
        else if (dell && strstr(name, " Touchpad") &&
                 !strcmp(name + strlen(name) - strlen(" Touchpad"), " Touchpad"))
            snprintf(pointer, sizeof pointer, "%s", event);
        else if (dell && !strcmp(name, "CUST0000:00 04F3:2A4B"))
            snprintf(touchscreen, sizeof touchscreen, "%s", event);
    }
    globfree(&entries);
    if (!keyboard[0] || !pointer[0] || (dell && !touchscreen[0]) ||
        !classify(keyboard, "keyboard") ||
        (dell && !classify(touchscreen, "touchscreen")) ||
        !classify(pointer, dell ? "touchpad" : "pointer"))
        return fprintf(stderr, "Expected input unavailable\n"), 1;
    char temporary[512];
    if (snprintf(temporary, sizeof temporary, "%s.new.XXXXXX", output) >= (int)sizeof temporary)
        return 1;
    int fd = mkstemp(temporary);
    if (fd < 0) return perror("xorg config"), 1;
    FILE *file = fdopen(fd, "w");
    if (!file) { close(fd); unlink(temporary); return 1; }
    fprintf(file,
        "Section \"ServerFlags\"\n"
        " Option \"AutoAddDevices\" \"false\"\n"
        " Option \"AutoAddGPU\" \"false\"\n"
        " Option \"BlankTime\" \"0\"\n"
        " Option \"StandbyTime\" \"0\"\n"
        " Option \"SuspendTime\" \"0\"\n"
        " Option \"OffTime\" \"0\"\n"
        "EndSection\n"
        "Section \"Device\"\n"
        " Identifier \"%s\"\n"
        " Driver \"%s\"\n"
        " Option \"AccelMethod\" \"none\"\n"
        " Option \"%s\" \"%s\"\n"
        "EndSection\n"
        "Section \"Screen\"\n"
        " Identifier \"screen\"\n"
        " Device \"%s\"\n"
        "EndSection\n", vm ? "hyperv" : "intel", vm ? "fbdev" : "modesetting",
        vm ? "fbdev" : "kmsdev", vm ? "/dev/fb0" : display_path,
        vm ? "hyperv" : "intel");
    const char *ids[] = {"keyboard", dell ? "touchscreen" : "pointer", "touchpad"};
    const char *events[] = {keyboard, dell ? touchscreen : pointer, pointer};
    const char *roles[] = {"CoreKeyboard", dell ? "SendCoreEvents" : "CorePointer", "CorePointer"};
    int count = dell ? 3 : 2;
    for (int i = 0; i < count; i++)
        fprintf(file,
            "Section \"InputDevice\"\n"
            " Identifier \"%s\"\n"
            " Driver \"libinput\"\n"
            " Option \"Device\" \"/dev/input/%s\"\n"
            " Option \"Tapping\" \"true\"\n"
            " Option \"NaturalScrolling\" \"true\"\n"
            " Option \"ScrollMethod\" \"twofinger\"\n"
            " Option \"DisableWhileTyping\" \"true\"\n"
            "EndSection\n", ids[i], events[i]);
    fprintf(file,
        "Section \"ServerLayout\"\n"
        " Identifier \"companion\"\n"
        " Screen \"screen\"\n");
    for (int i = 0; i < count; i++)
        fprintf(file, " InputDevice \"%s\" \"%s\"\n", ids[i], roles[i]);
    fprintf(file, "EndSection\n");
    bool good = fflush(file) == 0 && fsync(fd) == 0;
    if (fclose(file)) good = false;
    if (good) good = rename(temporary, output) == 0;
    if (!good) { unlink(temporary); return perror("xorg config"), 1; }
    printf("Companion %s Xorg config: %s, %s\n", vm ? "VM" : "Dell", keyboard, pointer);
    return 0;
}

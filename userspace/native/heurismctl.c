#define _POSIX_C_SOURCE 200809L
/* Small local client for the Heurism control socket. */
#include <errno.h>
#include <json-c/json.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define DEFAULT_SOCKET "/run/heurism-desktop/control.sock"

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

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism control client 0.1 (C)"); return 0;
    }
    const char *path = DEFAULT_SOCKET;
    int index = 1;
    if (argc > 2 && !strcmp(argv[1], "--socket")) { path = argv[2]; index = 3; }
    if (argc - index < 1 || argc - index > 2) {
        fprintf(stderr, "usage: heurismctl [--socket path] action [json-value]\n"); return 2;
    }
    struct json_object *request = json_object_new_object();
    json_object_object_add(request, "version", json_object_new_int(1));
    json_object_object_add(request, "action", json_object_new_string(argv[index]));
    if (argc - index == 2) {
        struct json_object *value = json_tokener_parse(argv[index + 1]);
        if (!value) return fprintf(stderr, "invalid JSON value\n"), 2;
        json_object_object_add(request, "value", value);
    }
    const char *encoded = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
    if (strlen(encoded) > 4000) return fprintf(stderr, "request too large\n"), 2;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (strlen(path) >= sizeof address.sun_path) return fprintf(stderr, "socket path too long\n"), 2;
    strcpy(address.sun_path, path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return perror("socket"), 1;
    if (connect(fd, (struct sockaddr *)&address, sizeof address)) return perror("connect"), 1;
    if (!write_all(fd, encoded, strlen(encoded)) || !write_all(fd, "\n", 1))
        return perror("write"), 1;
    char response[65536];
    size_t used = 0;
    while (used < sizeof response - 1) {
        struct pollfd input = {.fd = fd, .events = POLLIN};
        if (poll(&input, 1, 45000) <= 0) return fprintf(stderr, "control response timeout\n"), 1;
        ssize_t n = read(fd, response + used, 1);
        if (n <= 0) return fprintf(stderr, "control response ended early\n"), 1;
        if (response[used++] == '\n') break;
    }
    close(fd);
    json_object_put(request);
    if (!used || response[used - 1] != '\n') return fprintf(stderr, "control response too long\n"), 1;
    response[used] = 0;
    struct json_object *parsed = json_tokener_parse(response), *okay = NULL;
    if (!parsed || !json_object_object_get_ex(parsed, "ok", &okay))
        return fprintf(stderr, "invalid control response\n"), 1;
    fputs(response, stdout);
    int result = json_object_get_boolean(okay) ? 0 : 1;
    json_object_put(parsed);
    return result;
}

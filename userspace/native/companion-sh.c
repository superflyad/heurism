#define _POSIX_C_SOURCE 200809L
/* Companion's small Unix command shell. No graphics, privilege changes, or network API. */
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_LINE 8192
#define MAX_TEXT 16384
#define MAX_TOKENS 256
#define MAX_ARGS 128
#define MAX_CMDS 16
#define HISTORY_SIZE 64

enum kind { WORD, PIPE, SEMI, INPUT, OUTPUT, APPEND, ERROR_OUT, ERROR_APPEND };
struct token { enum kind kind; char *text; bool expand_glob; };
struct command {
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input, *output, *error;
    bool append_output, append_error;
};
struct pipeline { struct command commands[MAX_CMDS]; int count; };
static int last_status;
static bool shell_exit;
static volatile sig_atomic_t interrupted;
struct history { char *lines[HISTORY_SIZE]; int count; };

static void on_interrupt(int signal_number) {
    (void)signal_number;
    interrupted = 1;
}

static size_t previous_character(const char *line, size_t cursor) {
    if (!cursor) return 0;
    cursor--;
    while (cursor && ((unsigned char)line[cursor] & 0xc0) == 0x80) cursor--;
    return cursor;
}

static size_t next_character(const char *line, size_t length, size_t cursor) {
    if (cursor >= length) return length;
    cursor++;
    while (cursor < length && ((unsigned char)line[cursor] & 0xc0) == 0x80) cursor++;
    return cursor;
}

static void show_line(const char *prompt, const char *line, size_t length, size_t cursor) {
    fputs("\r\033[2K", stdout);
    fputs(prompt, stdout);
    fwrite(line, 1, length, stdout);
    if (cursor != length) {
        putchar('\r');
        fputs(prompt, stdout);
        fwrite(line, 1, cursor, stdout);
    }
    fflush(stdout);
}

static int escape_byte(void) {
    struct pollfd input = {.fd = STDIN_FILENO, .events = POLLIN};
    if (poll(&input, 1, 80) != 1) return -1;
    unsigned char byte;
    return read(STDIN_FILENO, &byte, 1) == 1 ? byte : -1;
}

/* Session history is intentionally not written to disk: commands may contain secrets. */
static void remember_line(struct history *history, const char *line, size_t length) {
    if (!length || (history->count && strlen(history->lines[history->count - 1]) == length &&
                    !memcmp(history->lines[history->count - 1], line, length))) return;
    char *copy = strndup(line, length);
    if (!copy) return;
    if (history->count == HISTORY_SIZE) {
        free(history->lines[0]);
        memmove(history->lines, history->lines + 1,
                (HISTORY_SIZE - 1) * sizeof history->lines[0]);
        history->count--;
    }
    history->lines[history->count++] = copy;
}

/* Return 1 for a line, 0 for EOF, and -1 for an interrupted line. */
static int read_interactive_line(char *line, size_t capacity, const char *prompt,
                                 struct history *history) {
    struct termios saved, edited;
    if (tcgetattr(STDIN_FILENO, &saved)) return -1;
    edited = saved;
    edited.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    edited.c_iflag &= (tcflag_t)~IXON;
    edited.c_cc[VMIN] = 1;
    edited.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &edited)) return -1;
    size_t length = 0, cursor = 0, draft_length = 0;
    int history_index = history->count, result = 0;
    char draft[MAX_LINE + 1] = {0};
    line[0] = 0;
    for (;;) {
        unsigned char byte;
        ssize_t count = read(STDIN_FILENO, &byte, 1);
        if (count < 0 && errno == EINTR) {
            if (interrupted) { result = -1; break; }
            continue;
        }
        if (count <= 0) break;
        if (byte == '\r' || byte == '\n') {
            remember_line(history, line, length);
            line[length++] = '\n'; line[length] = 0;
            fputs("\r\n", stdout); fflush(stdout);
            result = 1; break;
        }
        if (byte == 3) { interrupted = 1; result = -1; break; }
        if (byte == 4 && !length) break;
        if (byte == 1) cursor = 0;
        else if (byte == 5) cursor = length;
        else if (byte == 21) {
            memmove(line, line + cursor, length - cursor + 1);
            length -= cursor; cursor = 0;
        } else if (byte == 11) line[length = cursor] = 0;
        else if (byte == 127 || byte == 8) {
            if (cursor) {
                size_t prior = previous_character(line, cursor);
                memmove(line + prior, line + cursor, length - cursor + 1);
                length -= cursor - prior; cursor = prior;
            }
        } else if (byte == 27) {
            if (escape_byte() != '[') continue;
            int key = escape_byte();
            if (key == 'A' || key == 'B') {
                if (key == 'A' && history_index > 0) {
                    if (history_index == history->count) {
                        memcpy(draft, line, length + 1); draft_length = length;
                    }
                    history_index--;
                } else if (key == 'B' && history_index < history->count) history_index++;
                else continue;
                const char *selected = history_index == history->count ? draft :
                                       history->lines[history_index];
                length = history_index == history->count ? draft_length : strlen(selected);
                memcpy(line, selected, length + 1); cursor = length;
            } else if (key == 'C') cursor = next_character(line, length, cursor);
            else if (key == 'D') cursor = previous_character(line, cursor);
            else if (key == 'H') cursor = 0;
            else if (key == 'F') cursor = length;
            else if (key == '3' && escape_byte() == '~' && cursor < length) {
                size_t next = next_character(line, length, cursor);
                memmove(line + cursor, line + next, length - next + 1);
                length -= next - cursor;
            }
        } else if (byte >= 32 && byte != 127 && length + 1 < capacity) {
            bool at_end = cursor == length;
            memmove(line + cursor + 1, line + cursor, length - cursor + 1);
            line[cursor++] = (char)byte; length++;
            if (at_end) { putchar(byte); fflush(stdout); continue; }
        } else if (byte == 4 && cursor < length) {
            size_t next = next_character(line, length, cursor);
            memmove(line + cursor, line + next, length - next + 1);
            length -= next - cursor;
        }
        show_line(prompt, line, length, cursor);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    return result;
}

static void free_tokens(struct token *tokens, int count) {
    for (int i = 0; i < count; i++) free(tokens[i].text);
}

static bool add_text(char *buf, size_t *len, const char *text, size_t count) {
    if (count > MAX_TEXT - *len - 1) return false;
    memcpy(buf + *len, text, count);
    *len += count;
    buf[*len] = 0;
    return true;
}

static bool expand_variable(const char **cursor, char *buf, size_t *len) {
    const char *s = *cursor + 1;
    char name[256];
    const char *value = NULL;
    if (*s == '?') {
        snprintf(name, sizeof name, "%d", last_status);
        value = name;
        s++;
    } else if (*s == '$') {
        snprintf(name, sizeof name, "%ld", (long)getpid());
        value = name;
        s++;
    } else if (isalpha((unsigned char)*s) || *s == '_') {
        size_t n = 0;
        while (isalnum((unsigned char)*s) || *s == '_') {
            if (n + 1 >= sizeof name) return false;
            name[n++] = *s++;
        }
        name[n] = 0;
        value = getenv(name);
        if (!value) value = "";
    } else {
        value = "$";
    }
    *cursor = s;
    return add_text(buf, len, value, strlen(value));
}

static bool lex(const char *line, struct token *tokens, int *count) {
    const char *s = line;
    *count = 0;
    while (*s) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') break;
        if (*count >= MAX_TOKENS) return false;
        enum kind kind = WORD;
        size_t op = 0;
        if (s[0] == '2' && s[1] == '>') {
            kind = s[2] == '>' ? ERROR_APPEND : ERROR_OUT;
            op = kind == ERROR_APPEND ? 3 : 2;
        } else if (*s == '|') kind = PIPE, op = 1;
        else if (*s == ';') kind = SEMI, op = 1;
        else if (*s == '<') kind = INPUT, op = 1;
        else if (*s == '>') kind = s[1] == '>' ? APPEND : OUTPUT, op = kind == APPEND ? 2 : 1;
        if (op) {
            tokens[(*count)++] = (struct token){.kind = kind};
            s += op;
            continue;
        }
        char buf[MAX_TEXT];
        size_t len = 0;
        buf[0] = 0;
        bool began = false;
        bool quoted = false, wildcard = false;
        while (*s && !isspace((unsigned char)*s) && *s != '|' && *s != ';' && *s != '<' && *s != '>') {
            if (s[0] == '2' && s[1] == '>' && !began) break;
            began = true;
            if (*s == '\'') {
                quoted = true;
                s++;
                while (*s && *s != '\'') {
                    if (!add_text(buf, &len, s++, 1)) return false;
                }
                if (!*s) return false;
                s++;
            } else if (*s == '"') {
                quoted = true;
                s++;
                while (*s && *s != '"') {
                    if (*s == '\\' && (s[1] == '"' || s[1] == '\\' || s[1] == '$')) s++;
                    if (*s == '$') {
                        if (!expand_variable(&s, buf, &len)) return false;
                    } else if (!add_text(buf, &len, s++, 1)) return false;
                }
                if (!*s) return false;
                s++;
            } else if (*s == '\\') {
                quoted = true;
                s++;
                if (!*s || !add_text(buf, &len, s++, 1)) return false;
            } else if (*s == '$') {
                if (!expand_variable(&s, buf, &len)) return false;
            } else if (*s == '~' && len == 0) {
                const char *home = getenv("HOME");
                if (home && (s[1] == '/' || s[1] == 0 || isspace((unsigned char)s[1]))) {
                    if (!add_text(buf, &len, home, strlen(home))) return false;
                    s++;
                } else if (!add_text(buf, &len, s++, 1)) return false;
            } else {
                if (*s == '*' || *s == '?' || *s == '[') wildcard = true;
                if (!add_text(buf, &len, s++, 1)) return false;
            }
        }
        if (!began) return false;
        tokens[(*count)++] = (struct token){.kind = WORD, .text = strdup(buf),
                                            .expand_glob = wildcard && !quoted};
        if (!tokens[*count - 1].text) return false;
    }
    return true;
}

static bool identifier(const char *name) {
    if (!name || !(isalpha((unsigned char)*name) || *name == '_')) return false;
    for (name++; *name; name++) if (!(isalnum((unsigned char)*name) || *name == '_')) return false;
    return true;
}

static int builtin(struct command *cmd, bool *handled) {
    const char *name = cmd->argv[0];
    *handled = true;
    if (!strcmp(name, "cd")) {
        if (cmd->argc > 2) return fprintf(stderr, "cd: too many arguments\n"), 2;
        const char *target = cmd->argc == 2 ? cmd->argv[1] : getenv("HOME");
        if (!target) return fprintf(stderr, "cd: HOME is unset\n"), 1;
        char *old = getcwd(NULL, 0);
        if (chdir(target)) { perror("cd"); free(old); return 1; }
        char *now = getcwd(NULL, 0);
        if (old) setenv("OLDPWD", old, 1);
        if (now) setenv("PWD", now, 1);
        free(old); free(now);
        return 0;
    }
    if (!strcmp(name, "pwd")) {
        if (cmd->argc != 1) return fprintf(stderr, "pwd: no arguments expected\n"), 2;
        char *cwd = getcwd(NULL, 0);
        if (!cwd) return perror("pwd"), 1;
        puts(cwd); free(cwd); return 0;
    }
    if (!strcmp(name, "exit")) {
        if (cmd->argc > 2) return fprintf(stderr, "exit: too many arguments\n"), 2;
        int code = last_status;
        if (cmd->argc == 2) {
            char *end;
            long value = strtol(cmd->argv[1], &end, 10);
            if (*end) return fprintf(stderr, "exit: integer required\n"), 2;
            code = (unsigned char)value;
        }
        shell_exit = true;
        return code;
    }
    if (!strcmp(name, "export")) {
        if (cmd->argc == 1) return 0;
        for (int i = 1; i < cmd->argc; i++) {
            char *item = cmd->argv[i], *equals = strchr(item, '=');
            if (!equals) {
                if (!identifier(item)) return fprintf(stderr, "export: invalid name\n"), 2;
                if (!getenv(item) && setenv(item, "", 0)) return perror("export"), 1;
            } else {
                *equals = 0;
                bool valid = identifier(item);
                *equals = '=';
                if (!valid) return fprintf(stderr, "export: invalid name\n"), 2;
                char namebuf[256];
                size_t n = (size_t)(equals - item);
                if (n >= sizeof namebuf) return fprintf(stderr, "export: name too long\n"), 2;
                memcpy(namebuf, item, n); namebuf[n] = 0;
                if (setenv(namebuf, equals + 1, 1)) return perror("export"), 1;
            }
        }
        return 0;
    }
    if (!strcmp(name, "unset")) {
        for (int i = 1; i < cmd->argc; i++) {
            if (!identifier(cmd->argv[i])) return fprintf(stderr, "unset: invalid name\n"), 2;
            if (unsetenv(cmd->argv[i])) return perror("unset"), 1;
        }
        return 0;
    }
    if (!strcmp(name, "help")) {
        puts("Companion shell: cd, pwd, export, unset, exit, help; Unix programs, pipes and redirection.");
        return 0;
    }
    *handled = false;
    return 0;
}

static int redirect_fd(const char *path, int target, bool append, bool input) {
    if (!path) return 0;
    int flags = input ? O_RDONLY : O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    int fd = open(path, flags, 0666);
    if (fd < 0) return perror(path), -1;
    if (dup2(fd, target) < 0) { perror("dup2"); close(fd); return -1; }
    close(fd);
    return 0;
}

static int apply_redirects(const struct command *cmd) {
    if (redirect_fd(cmd->input, STDIN_FILENO, false, true) < 0 ||
        redirect_fd(cmd->output, STDOUT_FILENO, cmd->append_output, false) < 0 ||
        redirect_fd(cmd->error, STDERR_FILENO, cmd->append_error, false) < 0) return -1;
    return 0;
}

static int run_builtin_parent(struct command *cmd) {
    int saved[3] = {dup(0), dup(1), dup(2)};
    if (saved[0] < 0 || saved[1] < 0 || saved[2] < 0) {
        perror("dup");
        for (int i = 0; i < 3; i++) if (saved[i] >= 0) close(saved[i]);
        return 1;
    }
    int status = 1;
    fflush(NULL);
    if (apply_redirects(cmd) == 0) {
        bool handled;
        status = builtin(cmd, &handled);
        fflush(NULL);
    }
    for (int i = 0; i < 3; i++) { dup2(saved[i], i); close(saved[i]); }
    return status;
}

static bool is_builtin(const char *name) {
    return !strcmp(name, "cd") || !strcmp(name, "pwd") || !strcmp(name, "export") ||
           !strcmp(name, "unset") || !strcmp(name, "exit") || !strcmp(name, "help");
}

static int run_pipeline(struct pipeline *p) {
    if (p->count == 1 && is_builtin(p->commands[0].argv[0]))
        return run_builtin_parent(&p->commands[0]);
    pid_t pids[MAX_CMDS];
    int previous = -1, spawned = 0;
    bool failed = false;
    for (int i = 0; i < p->count; i++) {
        int next[2] = {-1, -1};
        if (i + 1 < p->count && pipe(next)) { perror("pipe"); failed = true; break; }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (next[0] >= 0) close(next[0]), close(next[1]);
            failed = true;
            break;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            if (previous >= 0 && dup2(previous, STDIN_FILENO) < 0) _exit(1);
            if (next[1] >= 0 && dup2(next[1], STDOUT_FILENO) < 0) _exit(1);
            if (previous >= 0) close(previous);
            if (next[0] >= 0) close(next[0]), close(next[1]);
            struct command *cmd = &p->commands[i];
            if (apply_redirects(cmd) < 0) _exit(1);
            if (is_builtin(cmd->argv[0])) {
                bool handled;
                int code = builtin(cmd, &handled);
                fflush(NULL);
                _exit(code);
            }
            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "%s: %s\n", cmd->argv[0], strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }
        pids[spawned++] = pid;
        if (previous >= 0) close(previous);
        if (next[1] >= 0) close(next[1]);
        previous = next[0];
    }
    if (previous >= 0) close(previous);
    int code = 1;
    for (int i = 0; i < spawned; i++) {
        int status = 0;
        pid_t result;
        do { result = waitpid(pids[i], &status, 0); } while (result < 0 && errno == EINTR);
        if (i == spawned - 1) code = result < 0 ? 1 : WIFEXITED(status) ? WEXITSTATUS(status) :
                                     WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
    }
    return failed ? 1 : code;
}

static int execute_segment(const char *line) {
    struct token tokens[MAX_TOKENS] = {0};
    char *expanded[MAX_ARGS * MAX_CMDS];
    int expanded_count = 0;
    int count = 0;
    if (!lex(line, tokens, &count)) {
        fprintf(stderr, "companion-sh: invalid or oversized command line\n");
        free_tokens(tokens, count);
        return 2;
    }
    struct pipeline p = {0};
    p.count = 1;
    struct command *cmd = &p.commands[0];
    int status = last_status;
    for (int i = 0; i <= count; i++) {
        enum kind kind = i == count ? SEMI : tokens[i].kind;
        if (kind == WORD) {
            if (tokens[i].expand_glob) {
                glob_t matches = {0};
                int outcome = glob(tokens[i].text, 0, NULL, &matches);
                if (outcome && outcome != GLOB_NOMATCH) {
                    globfree(&matches); goto syntax;
                }
                if (!outcome) {
                    for (size_t j = 0; j < matches.gl_pathc; j++) {
                        if (cmd->argc >= MAX_ARGS ||
                            expanded_count >= (int)(sizeof expanded / sizeof expanded[0])) {
                            globfree(&matches); goto syntax;
                        }
                        char *copy = strdup(matches.gl_pathv[j]);
                        if (!copy) { globfree(&matches); goto syntax; }
                        expanded[expanded_count++] = copy;
                        cmd->argv[cmd->argc++] = copy;
                    }
                } else {
                    if (cmd->argc >= MAX_ARGS) { globfree(&matches); goto syntax; }
                    cmd->argv[cmd->argc++] = tokens[i].text;
                }
                globfree(&matches);
            } else {
                if (cmd->argc >= MAX_ARGS) goto syntax;
                cmd->argv[cmd->argc++] = tokens[i].text;
            }
        } else if (kind == INPUT || kind == OUTPUT || kind == APPEND || kind == ERROR_OUT || kind == ERROR_APPEND) {
            if (++i >= count || tokens[i].kind != WORD) goto syntax;
            char **path = kind == INPUT ? &cmd->input :
                          (kind == ERROR_OUT || kind == ERROR_APPEND) ? &cmd->error : &cmd->output;
            if (*path) goto syntax;
            *path = tokens[i].text;
            if (kind == APPEND) cmd->append_output = true;
            if (kind == ERROR_APPEND) cmd->append_error = true;
        } else if (kind == PIPE) {
            if (!cmd->argc || p.count >= MAX_CMDS) goto syntax;
            cmd->argv[cmd->argc] = NULL;
            cmd = &p.commands[p.count++];
        } else if (kind == SEMI) {
            if (!cmd->argc) {
                if (i == count && p.count == 1 && !count) break;
                goto syntax;
            }
            cmd->argv[cmd->argc] = NULL;
            status = run_pipeline(&p);
            break;
        }
    }
    for (int i = 0; i < expanded_count; i++) free(expanded[i]);
    free_tokens(tokens, count);
    return status;
syntax:
    fprintf(stderr, "companion-sh: syntax error\n");
    for (int i = 0; i < expanded_count; i++) free(expanded[i]);
    free_tokens(tokens, count);
    return 2;
}

static int execute(const char *line) {
    const char *start = line;
    char quote = 0;
    bool escaped = false;
    bool word_start = true;
    int status = last_status;
    for (const char *p = line; ; p++) {
        char ch = *p;
        bool comment = ch == '#' && word_start && !quote && !escaped;
        if (!ch || comment || (ch == ';' && !quote && !escaped)) {
            size_t n = (size_t)(p - start);
            char segment[MAX_LINE + 1];
            if (n > MAX_LINE) return fprintf(stderr, "companion-sh: command too long\n"), 2;
            memcpy(segment, start, n);
            segment[n] = 0;
            status = execute_segment(segment);
            last_status = status;
            if (!ch || comment || shell_exit) break;
            start = p + 1;
            escaped = false;
            word_start = true;
            continue;
        }
        if (escaped) { escaped = false; word_start = false; continue; }
        if (ch == '\\' && quote != '\'') { escaped = true; word_start = false; continue; }
        if ((ch == '\'' || ch == '"') && !quote) quote = ch;
        else if (ch == quote) quote = 0;
        if (!quote && isspace((unsigned char)ch)) word_start = true;
        else if (!quote && (ch == '|' || ch == '<' || ch == '>')) word_start = true;
        else word_start = false;
    }
    return status;
}

static char *prompt_text(void) {
    char *cwd = getcwd(NULL, 0);
    const char *place = cwd ? cwd : "?";
    const char *home = getenv("HOME");
    bool in_home = false;
    if (cwd && home && *home) {
        size_t n = strlen(home);
        if (!strncmp(cwd, home, n) && (cwd[n] == '/' || !cwd[n])) {
            place = cwd + n;
            in_home = true;
        }
    }
    char marker = geteuid() ? '$' : '#';
    size_t needed = strlen(place) + 32;
    char *prompt = malloc(needed);
    if (prompt) snprintf(prompt, needed, "companion:%s%s%c ", in_home ? "~" : "",
                         place, marker);
    free(cwd);
    return prompt;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Companion shell 0.1 (C/POSIX)"); return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "-c")) return execute(argv[2]);
    if (argc != 1) return fprintf(stderr, "usage: companion-sh [-c command]\n"), 2;
    bool interactive = isatty(STDIN_FILENO);
    if (interactive) {
        struct sigaction action = {.sa_handler = on_interrupt};
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
        signal(SIGQUIT, SIG_IGN);
    }
    char line[MAX_LINE + 2];
    struct history history = {0};
    while (!shell_exit) {
        int input = 1;
        if (interactive) {
            char *prompt = prompt_text();
            if (!prompt) break;
            fputs(prompt, stdout);
            fflush(stdout);
            input = read_interactive_line(line, sizeof line, prompt, &history);
            free(prompt);
        } else if (!fgets(line, sizeof line, stdin)) input = 0;
        if (input < 0) {
            putchar('\n');
            last_status = 130;
            interrupted = 0;
            continue;
        }
        if (!input) break;
        size_t n = strlen(line);
        if (n > MAX_LINE || (n && line[n - 1] != '\n' && !feof(stdin))) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            fprintf(stderr, "companion-sh: line exceeds %d bytes\n", MAX_LINE);
            last_status = 2;
            continue;
        }
        last_status = execute(line);
        if (interactive && interrupted) {
            putchar('\n');
            interrupted = 0;
        }
    }
    for (int i = 0; i < history.count; i++) free(history.lines[i]);
    return last_status;
}

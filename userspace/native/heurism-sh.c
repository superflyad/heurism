#define _POSIX_C_SOURCE 200809L
/* Heurism's small Unix command shell. No graphics, privilege changes, or network API. */
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
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 8192
#define MAX_TEXT 16384
#define MAX_TOKENS 256
#define MAX_ARGS 128
#define MAX_CMDS 16
#define HISTORY_SIZE 64
#define MAX_JOBS 32

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
static volatile sig_atomic_t hangup_requested;
struct history { char *lines[HISTORY_SIZE]; int count; };
struct job {
    pid_t pgid, pids[MAX_CMDS];
    unsigned char state[MAX_CMDS]; /* 0 running, 1 stopped, 2 finished */
    int count, last_status;
    unsigned long serial;
    char *description;
};
static struct job jobs[MAX_JOBS];
static unsigned long next_job_serial;
static bool interactive_shell;
static pid_t shell_pgid;

static bool job_done(const struct job *job) {
    for (int i = 0; i < job->count; i++) if (job->state[i] != 2) return false;
    return true;
}

static bool job_stopped(const struct job *job) {
    bool stopped = false;
    for (int i = 0; i < job->count; i++) {
        if (job->state[i] == 0) return false;
        if (job->state[i] == 1) stopped = true;
    }
    return stopped;
}

static void job_clear(struct job *job) {
    free(job->description);
    memset(job, 0, sizeof *job);
}

static void job_status(struct job *job, pid_t pid, int status) {
    for (int i = 0; i < job->count; i++) {
        if (job->pids[i] != pid) continue;
        if (WIFSTOPPED(status)) job->state[i] = 1;
        else if (WIFCONTINUED(status)) job->state[i] = 0;
        else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->state[i] = 2;
            if (i == job->count - 1)
                job->last_status = WIFEXITED(status) ? WEXITSTATUS(status) :
                                   128 + WTERMSIG(status);
        }
        return;
    }
}

static void reap_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        struct job *job = &jobs[i];
        if (!job->pgid) continue;
        int status;
        pid_t pid;
        while ((pid = waitpid(-job->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
            job_status(job, pid, status);
        if (job_done(job)) {
            if (interactive_shell) printf("[%d] Done %s\n", i + 1, job->description);
            job_clear(job);
        }
    }
}

static int wait_foreground(struct job *job, int number) {
    int status;
    pid_t pid;
    while (!job_done(job) && !job_stopped(job)) {
        pid = waitpid(-job->pgid, &status, WUNTRACED);
        if (pid > 0) job_status(job, pid, status);
        else if (errno != EINTR || hangup_requested) break;
    }
    if (interactive_shell && !hangup_requested && tcsetpgrp(STDIN_FILENO, shell_pgid))
        perror("tcsetpgrp");
    if (hangup_requested) return 128 + SIGHUP;
    if (job_stopped(job)) {
        printf("[%d] Stopped %s\n", number, job->description);
        return 128 + SIGTSTP;
    }
    int code = job->last_status;
    job_clear(job);
    return code;
}

static int select_job(const struct command *cmd) {
    if (cmd->argc > 2) return fprintf(stderr, "%s: too many arguments\n", cmd->argv[0]), -1;
    if (cmd->argc == 2) {
        const char *value = cmd->argv[1];
        if (*value == '%') value++;
        char *end;
        long number = strtol(value, &end, 10);
        if (!*value || *end || number < 1 || number > MAX_JOBS || !jobs[number - 1].pgid)
            return fprintf(stderr, "%s: no such job\n", cmd->argv[0]), -1;
        return (int)number - 1;
    }
    int newest = -1;
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].pgid && (newest < 0 || jobs[i].serial > jobs[newest].serial))
            newest = i;
    if (newest >= 0) return newest;
    return fprintf(stderr, "%s: no current job\n", cmd->argv[0]), -1;
}

static void on_interrupt(int signal_number) {
    (void)signal_number;
    interrupted = 1;
}

static void on_hangup(int signal_number) {
    (void)signal_number;
    hangup_requested = 1;
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

struct completion {
    char *first;
    size_t common, count;
    bool directory;
};

static void completion_add(struct completion *matches, const char *name, bool directory) {
    if (matches->first && !strcmp(matches->first, name)) return;
    if (!matches->first) {
        matches->first = strdup(name);
        if (!matches->first) return;
        matches->common = strlen(name);
        matches->directory = directory;
    } else {
        size_t n = 0;
        while (n < matches->common && name[n] && matches->first[n] == name[n]) n++;
        matches->common = n;
    }
    matches->count++;
}

static void complete_directory(struct completion *matches, const char *directory,
                               const char *prefix, bool executable_only,
                               bool include_directories) {
    DIR *stream = opendir(*directory ? directory : ".");
    if (!stream) return;
    size_t n = strlen(prefix), base = strlen(directory);
    struct dirent *entry;
    while ((entry = readdir(stream))) {
        const char *name = entry->d_name;
        if (strncmp(name, prefix, n) || (!n && name[0] == '.')) continue;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        bool printable = true;
        for (const unsigned char *s = (const unsigned char *)name; *s; s++)
            if (*s < 32 || *s == 127) printable = false;
        if (!printable) continue;
        size_t length = base + strlen(name) + 2;
        char *path = malloc(length);
        if (!path) break;
        snprintf(path, length, "%s/%s", *directory ? directory : ".", name);
        struct stat info;
        if (!stat(path, &info) &&
            ((include_directories && S_ISDIR(info.st_mode)) ||
             (S_ISREG(info.st_mode) && (!executable_only || !access(path, X_OK)))))
            completion_add(matches, name, S_ISDIR(info.st_mode));
        free(path);
    }
    closedir(stream);
}

static bool completion_escape(char *output, size_t capacity, size_t *used, const char *text,
                              size_t count) {
    for (size_t i = 0; i < count; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (isspace(ch) || strchr("\\\"'$;|&<>*?[]#", ch)) {
            if (*used + 1 >= capacity) return false;
            output[(*used)++] = '\\';
        }
        if (*used + 1 >= capacity) return false;
        output[(*used)++] = (char)ch;
    }
    return true;
}

/* Complete only an unquoted word at the line end. Ambiguous names extend to
 * their common prefix; path lookup is read-only and never executes a match. */
static bool complete_line(char *line, size_t capacity, size_t *length, size_t *cursor) {
    if (*cursor != *length) return false;
    size_t start = *cursor;
    while (start && !isspace((unsigned char)line[start - 1]) &&
           !strchr("|;&<>", line[start - 1])) start--;
    if (start == *cursor) return false;
    char *word = strndup(line + start, *cursor - start);
    if (!word) return false;
    if (strpbrk(word, "\\\"'$*?[]") || (word[0] == '~' && word[1] != '/')) {
        free(word); return false;
    }
    bool command = true;
    for (size_t i = 0; i < start; i++) {
        if (strchr("|;&", line[i])) command = true;
        else if (!isspace((unsigned char)line[i])) command = false;
    }
    struct completion matches = {0};
    char *slash = strrchr(word, '/');
    if (command && !slash) {
        const char *builtins[] = {"cd", "pwd", "exit", "export", "unset",
                                  "jobs", "fg", "bg", "help"};
        for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
            if (!strncmp(builtins[i], word, strlen(word)))
                completion_add(&matches, builtins[i], false);
        const char *path = getenv("PATH");
        if (path) {
            for (;;) {
                const char *end = strchr(path, ':');
                size_t size = end ? (size_t)(end - path) : strlen(path);
                char *directory = strndup(path, size);
                if (directory) {
                    complete_directory(&matches, directory, word, true, false);
                    free(directory);
                }
                if (!end) break;
                path = end + 1;
            }
        }
    } else {
        char *directory = slash ? strndup(word, (size_t)(slash - word)) : strdup(".");
        if (directory) {
            if (!*directory) { free(directory); directory = strdup("/"); }
            else if (directory[0] == '~' && directory[1] == '/') {
                const char *home = getenv("HOME");
                char *expanded = NULL;
                if (home) {
                    size_t size = strlen(home) + strlen(directory);
                    expanded = malloc(size);
                    if (expanded) snprintf(expanded, size, "%s%s", home, directory + 1);
                }
                free(directory); directory = expanded;
            }
            if (directory) complete_directory(&matches, directory,
                                              slash ? slash + 1 : word, command, true);
            free(directory);
        }
    }
    size_t typed = strlen(slash ? slash + 1 : word);
    char insert[MAX_LINE + 2];
    size_t used = 0;
    bool changed = false;
    if (matches.count && matches.common >= typed &&
        completion_escape(insert, sizeof insert, &used, matches.first + typed,
                          matches.common - typed)) {
        if (matches.count == 1) insert[used++] = matches.directory ? '/' : ' ';
        if (*length + used + 1 < capacity && *length + used <= MAX_LINE) {
            memcpy(line + *length, insert, used);
            *length += used; *cursor = *length;
            line[*length] = 0;
            changed = used != 0;
        }
    }
    free(matches.first);
    free(word);
    return changed;
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
            if (hangup_requested) break;
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
        if (byte == '\t') {
            if (!complete_line(line, capacity, &length, &cursor)) putchar('\a');
        } else if (byte == 1) cursor = 0;
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
    if (!strcmp(name, "jobs")) {
        if (cmd->argc != 1) return fprintf(stderr, "jobs: no arguments expected\n"), 2;
        reap_jobs();
        for (int i = 0; i < MAX_JOBS; i++)
            if (jobs[i].pgid) printf("[%d] %s %s\n", i + 1,
                                     job_stopped(&jobs[i]) ? "Stopped" : "Running",
                                     jobs[i].description);
        return 0;
    }
    if (!strcmp(name, "fg") || !strcmp(name, "bg")) {
        reap_jobs();
        int index = select_job(cmd);
        if (index < 0) return 1;
        struct job *job = &jobs[index];
        if (!strcmp(name, "fg") && interactive_shell &&
            tcsetpgrp(STDIN_FILENO, job->pgid)) return perror("tcsetpgrp"), 1;
        for (int i = 0; i < job->count; i++)
            if (job->state[i] == 1) job->state[i] = 0;
        if (kill(-job->pgid, SIGCONT) && errno != ESRCH) {
            perror("SIGCONT");
            if (interactive_shell && !strcmp(name, "fg")) tcsetpgrp(STDIN_FILENO, shell_pgid);
            return 1;
        }
        if (!strcmp(name, "bg")) {
            printf("[%d] Running %s\n", index + 1, job->description);
            return 0;
        }
        printf("%s\n", job->description);
        return wait_foreground(job, index + 1);
    }
    if (!strcmp(name, "help")) {
        puts("Heurism shell: cd, pwd, export, unset, jobs, fg, bg, exit, help; Unix programs, pipes and redirection.");
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
           !strcmp(name, "unset") || !strcmp(name, "exit") || !strcmp(name, "help") ||
           !strcmp(name, "jobs") || !strcmp(name, "fg") || !strcmp(name, "bg");
}

static bool is_job_builtin(const char *name) {
    return !strcmp(name, "jobs") || !strcmp(name, "fg") || !strcmp(name, "bg");
}

static int run_pipeline(struct pipeline *p, bool background, const char *description) {
    for (int i = 0; i < p->count; i++)
        if (is_job_builtin(p->commands[i].argv[0]) && (background || p->count != 1))
            return fprintf(stderr, "%s: requires the foreground shell\n",
                           p->commands[i].argv[0]), 2;
    if (!background && p->count == 1 && is_builtin(p->commands[0].argv[0]))
        return run_builtin_parent(&p->commands[0]);
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) if (!jobs[i].pgid) { slot = i; break; }
    if (slot < 0) return fprintf(stderr, "heurism-sh: job table full\n"), 1;
    struct job *job = &jobs[slot];
    job->description = strdup(description);
    if (!job->description) return perror("strdup"), 1;
    job->serial = ++next_job_serial;
    int gate[2];
    if (pipe(gate)) { perror("pipe"); job_clear(job); return 1; }
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
            pid_t group = job->pgid ? job->pgid : getpid();
            if (setpgid(0, group)) _exit(1);
            close(gate[1]);
            char release;
            while (read(gate[0], &release, 1) < 0 && errno == EINTR) {}
            close(gate[0]);
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGHUP, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
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
        if (!job->pgid) job->pgid = pid;
        if (setpgid(pid, job->pgid) && errno != EACCES && errno != ESRCH) {
            perror("setpgid"); failed = true;
        }
        job->pids[job->count++] = pid;
        spawned++;
        if (previous >= 0) close(previous);
        if (next[1] >= 0) close(next[1]);
        previous = next[0];
        if (failed) break;
    }
    if (previous >= 0) close(previous);
    close(gate[0]);
    if (failed) {
        if (job->pgid) kill(-job->pgid, SIGTERM);
        close(gate[1]);
        for (int i = 0; i < spawned; i++) waitpid(job->pids[i], NULL, 0);
        job_clear(job);
        return 1;
    }
    if (background) {
        close(gate[1]);
        if (interactive_shell) printf("[%d] %ld\n", slot + 1, (long)job->pgid);
        return 0;
    }
    if (interactive_shell && tcsetpgrp(STDIN_FILENO, job->pgid)) {
        perror("tcsetpgrp");
        kill(-job->pgid, SIGTERM);
        close(gate[1]);
        for (int i = 0; i < spawned; i++) waitpid(job->pids[i], NULL, 0);
        job_clear(job);
        return 1;
    }
    close(gate[1]);
    return wait_foreground(job, slot + 1);
}

static int execute_segment(const char *line, bool background) {
    struct token tokens[MAX_TOKENS] = {0};
    char *expanded[MAX_ARGS * MAX_CMDS];
    int expanded_count = 0;
    int count = 0;
    if (!lex(line, tokens, &count)) {
        fprintf(stderr, "heurism-sh: invalid or oversized command line\n");
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
            status = run_pipeline(&p, background, line);
            break;
        }
    }
    for (int i = 0; i < expanded_count; i++) free(expanded[i]);
    free_tokens(tokens, count);
    return status;
syntax:
    fprintf(stderr, "heurism-sh: syntax error\n");
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
        if (!ch || comment || ((ch == ';' || ch == '&') && !quote && !escaped)) {
            size_t n = (size_t)(p - start);
            char segment[MAX_LINE + 1];
            if (n > MAX_LINE) return fprintf(stderr, "heurism-sh: command too long\n"), 2;
            memcpy(segment, start, n);
            segment[n] = 0;
            status = execute_segment(segment, ch == '&');
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
    if (prompt) snprintf(prompt, needed, "heurism:%s%s%c ", in_home ? "~" : "",
                         place, marker);
    free(cwd);
    return prompt;
}

static void shutdown_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].pgid) {
        kill(-jobs[i].pgid, SIGHUP);
        kill(-jobs[i].pgid, SIGCONT);
        job_clear(&jobs[i]);
    }
}

int main(int argc, char **argv) {
    struct sigaction hangup = {.sa_handler = on_hangup};
    sigemptyset(&hangup.sa_mask);
    sigaction(SIGHUP, &hangup, NULL);
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism shell 0.1 (C/POSIX)"); return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "-c")) {
        int status = execute(argv[2]);
        shutdown_jobs();
        return status;
    }
    if (argc != 1) return fprintf(stderr, "usage: heurism-sh [-c command]\n"), 2;
    interactive_shell = isatty(STDIN_FILENO);
    if (interactive_shell) {
        shell_pgid = getpgrp();
        if (tcgetpgrp(STDIN_FILENO) != shell_pgid)
            return fprintf(stderr, "heurism-sh: terminal is not in foreground\n"), 1;
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        struct sigaction action = {.sa_handler = on_interrupt};
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
        signal(SIGQUIT, SIG_IGN);
    }
    char line[MAX_LINE + 2];
    struct history history = {0};
    while (!shell_exit) {
        if (hangup_requested) break;
        int input = 1;
        reap_jobs();
        if (interactive_shell) {
            char *prompt = prompt_text();
            if (!prompt) break;
            fputs(prompt, stdout);
            fflush(stdout);
            input = read_interactive_line(line, sizeof line, prompt, &history);
            free(prompt);
        } else if (!fgets(line, sizeof line, stdin)) input = 0;
        if (hangup_requested) break;
        if (input < 0) {
            putchar('\n');
            last_status = 130;
            interrupted = 0;
            continue;
        }
        if (!input) break;
        size_t n = strlen(line);
        bool complete = n && line[n - 1] == '\n';
        if (n > MAX_LINE || (n && !complete && !feof(stdin))) {
            if (!interactive_shell && !complete) {
                int ch;
                while ((ch = getchar()) != '\n' && ch != EOF) {}
            }
            fprintf(stderr, "heurism-sh: line exceeds %d bytes\n", MAX_LINE);
            last_status = 2;
            continue;
        }
        last_status = execute(line);
        if (interactive_shell && interrupted) {
            putchar('\n');
            interrupted = 0;
        }
    }
    shutdown_jobs();
    for (int i = 0; i < history.count; i++) free(history.lines[i]);
    return last_status;
}

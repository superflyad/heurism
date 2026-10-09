#define _GNU_SOURCE
/* Compact X11/PTy terminal. libvterm owns VT parsing; Heurism owns the process and window. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <vterm.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHELL_PATH "/opt/heurism/native/current/heurism-sh"
#define OUT_CAPACITY 65536
#define MARGIN 12
#define HISTORY_CAPACITY 512

struct history_line {
    VTermScreenCell *cells;
    int cols;
};

struct terminal {
    Display *display;
    int screen_number;
    Window window;
    Atom wm_delete;
    Visual *visual;
    GC gc;
    XftFont *font;
    XftDraw *draw;
    VTerm *vt;
    VTermScreen *screen;
    int master;
    pid_t child;
    int width, height, cell_width, cell_height, rows, cols;
    char outgoing[OUT_CAPACITY];
    size_t outgoing_length;
    struct history_line history[HISTORY_CAPACITY];
    size_t history_head, history_count, view_offset;
    bool running;
};

static struct history_line *history_at(struct terminal *t, size_t index) {
    return &t->history[(t->history_head + index) % HISTORY_CAPACITY];
}

static int history_push(int cols, const VTermScreenCell *cells, void *context) {
    struct terminal *t = context;
    if (cols <= 0 || cols > 300) return 0;
    VTermScreenCell *copy = malloc((size_t)cols * sizeof *copy);
    if (!copy) return 0;
    memcpy(copy, cells, (size_t)cols * sizeof *copy);
    if (t->history_count == HISTORY_CAPACITY) {
        struct history_line *oldest = history_at(t, 0);
        free(oldest->cells);
        oldest->cells = NULL;
        t->history_head = (t->history_head + 1) % HISTORY_CAPACITY;
        t->history_count--;
    }
    *history_at(t, t->history_count++) = (struct history_line){copy, cols};
    if (t->view_offset && t->view_offset < t->history_count) t->view_offset++;
    return 1;
}

static int history_pop(int cols, VTermScreenCell *cells, void *context) {
    struct terminal *t = context;
    if (!t->history_count || cols <= 0) return 0;
    struct history_line *line = history_at(t, t->history_count - 1);
    memset(cells, 0, (size_t)cols * sizeof *cells);
    /* Resized blank cells must be traversable and retain the default theme. */
    for (int i = 0; i < cols; i++) {
        cells[i].width = 1;
        cells[i].fg.type = VTERM_COLOR_DEFAULT_FG;
        cells[i].bg.type = VTERM_COLOR_DEFAULT_BG;
    }
    int copied = cols < line->cols ? cols : line->cols;
    memcpy(cells, line->cells, (size_t)copied * sizeof *cells);
    free(line->cells);
    *line = (struct history_line){0};
    t->history_count--;
    if (t->view_offset > t->history_count) t->view_offset = t->history_count;
    return 1;
}

static int history_clear(void *context) {
    struct terminal *t = context;
    for (size_t i = 0; i < t->history_count; i++) free(history_at(t, i)->cells);
    t->history_head = t->history_count = t->view_offset = 0;
    return 1;
}

static unsigned long component_pixel(unsigned value, unsigned long mask) {
    if (!mask) return 0;
    unsigned shift = 0;
    while (!(mask & 1)) { mask >>= 1; shift++; }
    return (((unsigned long)value * mask + 127) / 255) << shift;
}

static unsigned long pixel(struct terminal *t, VTermColor color) {
    vterm_screen_convert_color_to_rgb(t->screen, &color);
    return component_pixel(color.rgb.red, t->visual->red_mask) |
           component_pixel(color.rgb.green, t->visual->green_mask) |
           component_pixel(color.rgb.blue, t->visual->blue_mask);
}

static void redraw(struct terminal *t) {
    VTermColor default_bg = {.rgb = {VTERM_COLOR_RGB, 10, 19, 29}};
    XSetForeground(t->display, t->gc, pixel(t, default_bg));
    XFillRectangle(t->display, t->window, t->gc, 0, 0,
                   (unsigned)t->width, (unsigned)t->height);
    for (int row = 0; row < t->rows; row++) {
        size_t index = t->history_count - t->view_offset + (size_t)row;
        struct history_line *line = index < t->history_count ? history_at(t, index) : NULL;
        int live_row = (int)index - (int)t->history_count;
        for (int col = 0; col < t->cols; col++) {
            VTermScreenCell cell;
            if (line) {
                if (col >= line->cols) continue;
                cell = line->cells[col];
            } else if (!vterm_screen_get_cell(t->screen,
                       (VTermPos){live_row, col}, &cell)) continue;
            VTermColor foreground = cell.attrs.reverse ? cell.bg : cell.fg;
            VTermColor background = cell.attrs.reverse ? cell.fg : cell.bg;
            if (!VTERM_COLOR_IS_DEFAULT_BG(&background) || cell.attrs.reverse) {
                XSetForeground(t->display, t->gc, pixel(t, background));
                XFillRectangle(t->display, t->window, t->gc,
                               MARGIN + col * t->cell_width, MARGIN + row * t->cell_height,
                               (unsigned)t->cell_width, (unsigned)t->cell_height);
            }
            if (!cell.chars[0] || cell.attrs.conceal || cell.width == 0) continue;
            FcChar32 chars[VTERM_MAX_CHARS_PER_CELL];
            int length = 0;
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; i++)
                chars[length++] = cell.chars[i];
            VTermColor rgb = foreground;
            vterm_screen_convert_color_to_rgb(t->screen, &rgb);
            XftColor color = {.pixel = pixel(t, foreground),
                             .color = {(unsigned short)(rgb.rgb.red * 257),
                                       (unsigned short)(rgb.rgb.green * 257),
                                       (unsigned short)(rgb.rgb.blue * 257), 65535}};
            XftDrawString32(t->draw, &color, t->font,
                            MARGIN + col * t->cell_width,
                            MARGIN + row * t->cell_height + t->font->ascent,
                            chars, length);
            if (cell.attrs.underline) {
                XSetForeground(t->display, t->gc, color.pixel);
                XDrawLine(t->display, t->window, t->gc,
                          MARGIN + col * t->cell_width,
                          MARGIN + (row + 1) * t->cell_height - 2,
                          MARGIN + (col + 1) * t->cell_width - 1,
                          MARGIN + (row + 1) * t->cell_height - 2);
            }
        }
    }
    VTermPos cursor;
    vterm_state_get_cursorpos(vterm_obtain_state(t->vt), &cursor);
    if (!t->view_offset && cursor.row >= 0 && cursor.row < t->rows &&
        cursor.col >= 0 && cursor.col < t->cols) {
        XSetForeground(t->display, t->gc, 0x50e1be);
        XFillRectangle(t->display, t->window, t->gc,
                       MARGIN + cursor.col * t->cell_width,
                       MARGIN + (cursor.row + 1) * t->cell_height - 3,
                       (unsigned)t->cell_width, 3);
    }
    XFlush(t->display);
}

static void flush_outgoing(struct terminal *t) {
    while (t->outgoing_length) {
        ssize_t n = write(t->master, t->outgoing, t->outgoing_length);
        if (n > 0) {
            memmove(t->outgoing, t->outgoing + n, t->outgoing_length - (size_t)n);
            t->outgoing_length -= (size_t)n;
        } else if (n < 0 && errno == EINTR) continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        else { t->running = false; break; }
    }
}

static void terminal_output(const char *bytes, size_t length, void *context) {
    struct terminal *t = context;
    if (length > OUT_CAPACITY - t->outgoing_length) {
        fprintf(stderr, "heurism-terminal: PTY output queue full\n");
        t->running = false;
        return;
    }
    memcpy(t->outgoing + t->outgoing_length, bytes, length);
    t->outgoing_length += length;
    flush_outgoing(t);
}

static void resize_terminal(struct terminal *t, int width, int height) {
    t->width = width;
    t->height = height;
    int cols = (width - 2 * MARGIN) / t->cell_width;
    int rows = (height - 2 * MARGIN) / t->cell_height;
    if (cols < 20) cols = 20;
    if (rows < 5) rows = 5;
    if (cols > 300) cols = 300;
    if (rows > 120) rows = 120;
    if (cols != t->cols || rows != t->rows) {
        t->cols = cols;
        t->rows = rows;
        vterm_set_size(t->vt, rows, cols);
        struct winsize size = {.ws_row = (unsigned short)rows,
                               .ws_col = (unsigned short)cols,
                               .ws_xpixel = (unsigned short)width,
                               .ws_ypixel = (unsigned short)height};
        if (t->master >= 0) ioctl(t->master, TIOCSWINSZ, &size);
    }
    redraw(t);
}

static int decode_utf8(const unsigned char *s, int len, uint32_t *codepoint) {
    if (!len) return 0;
    if (s[0] < 0x80) { *codepoint = s[0]; return 1; }
    int count = s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 :
                s[0] >= 0xe0 && s[0] <= 0xef ? 3 :
                s[0] >= 0xc2 && s[0] <= 0xdf ? 2 : 0;
    if (!count || count > len) return -1;
    uint32_t value = s[0] & ((1u << (7 - count)) - 1);
    for (int i = 1; i < count; i++) {
        if ((s[i] & 0xc0) != 0x80) return -1;
        value = (value << 6) | (s[i] & 0x3f);
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
        (count == 2 && value < 0x80) || (count == 3 && value < 0x800) ||
        (count == 4 && value < 0x10000)) return -1;
    *codepoint = value;
    return count;
}

static VTermKey special_key(KeySym key) {
    switch (key) {
    case XK_Return: case XK_KP_Enter: return VTERM_KEY_ENTER;
    case XK_Tab: return VTERM_KEY_TAB;
    case XK_BackSpace: return VTERM_KEY_BACKSPACE;
    case XK_Escape: return VTERM_KEY_ESCAPE;
    case XK_Up: return VTERM_KEY_UP;
    case XK_Down: return VTERM_KEY_DOWN;
    case XK_Left: return VTERM_KEY_LEFT;
    case XK_Right: return VTERM_KEY_RIGHT;
    case XK_Insert: return VTERM_KEY_INS;
    case XK_Delete: return VTERM_KEY_DEL;
    case XK_Home: return VTERM_KEY_HOME;
    case XK_End: return VTERM_KEY_END;
    case XK_Page_Up: return VTERM_KEY_PAGEUP;
    case XK_Page_Down: return VTERM_KEY_PAGEDOWN;
    default:
        if (key >= XK_F1 && key <= XK_F12) return VTERM_KEY_FUNCTION((int)(key - XK_F1 + 1));
        return VTERM_KEY_NONE;
    }
}

static void scroll_view(struct terminal *t, int lines) {
    if (lines > 0) {
        size_t room = t->history_count - t->view_offset;
        t->view_offset += (size_t)lines < room ? (size_t)lines : room;
    } else if (lines < 0) {
        size_t amount = (size_t)(-lines);
        t->view_offset -= amount < t->view_offset ? amount : t->view_offset;
    }
    redraw(t);
}

static void keypress(struct terminal *t, XKeyEvent *event, XIC input_context) {
    char buffer[128];
    KeySym symbol = NoSymbol;
    int count;
    if (input_context) {
        Status status;
        count = Xutf8LookupString(input_context, event, buffer, sizeof buffer,
                                  &symbol, &status);
        if (status == XBufferOverflow) return;
    } else count = XLookupString(event, buffer, sizeof buffer, &symbol, NULL);
    if ((event->state & ShiftMask) && symbol == XK_Page_Up) {
        scroll_view(t, t->rows - 1);
        return;
    }
    if ((event->state & ShiftMask) && symbol == XK_Page_Down) {
        scroll_view(t, 1 - t->rows);
        return;
    }
    if (t->view_offset) { t->view_offset = 0; redraw(t); }
    VTermModifier modifier = VTERM_MOD_NONE;
    if (event->state & ShiftMask) modifier |= VTERM_MOD_SHIFT;
    if (event->state & ControlMask) modifier |= VTERM_MOD_CTRL;
    if (event->state & Mod1Mask) modifier |= VTERM_MOD_ALT;
    VTermKey key = special_key(symbol);
    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(t->vt, key, modifier);
    } else if ((event->state & ControlMask) &&
               ((symbol >= XK_a && symbol <= XK_z) ||
                (symbol >= XK_A && symbol <= XK_Z))) {
        vterm_keyboard_unichar(t->vt, (uint32_t)symbol, modifier);
    } else {
        for (int i = 0; i < count;) {
            uint32_t codepoint;
            int used = decode_utf8((const unsigned char *)buffer + i, count - i, &codepoint);
            if (used < 0) break;
            vterm_keyboard_unichar(t->vt, codepoint, modifier);
            i += used;
        }
    }
    flush_outgoing(t);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("Heurism terminal 0.1 (C/X11/libvterm)"); return 0;
    }
    if (argc != 1) return fprintf(stderr, "usage: heurism-terminal\n"), 2;
    setlocale(LC_CTYPE, "");
    struct terminal t = {.master = -1, .running = true};
    t.display = XOpenDisplay(NULL);
    if (!t.display) return fprintf(stderr, "heurism-terminal: no X display\n"), 1;
    t.screen_number = DefaultScreen(t.display);
    t.visual = DefaultVisual(t.display, t.screen_number);
    if (t.visual->class != TrueColor) return fprintf(stderr, "heurism-terminal: TrueColor display required\n"), 1;
    t.font = XftFontOpenName(t.display, t.screen_number, "DejaVu Sans Mono:size=13");
    if (!t.font) return fprintf(stderr, "heurism-terminal: font unavailable\n"), 1;
    t.cell_width = t.font->max_advance_width;
    t.cell_height = t.font->ascent + t.font->descent + 3;
    if (t.cell_width < 1 || t.cell_height < 1) return 1;
    t.width = 920; t.height = 580;
    t.cols = (t.width - 2 * MARGIN) / t.cell_width;
    t.rows = (t.height - 2 * MARGIN) / t.cell_height;
    t.vt = vterm_new(t.rows, t.cols);
    if (!t.vt) return fprintf(stderr, "heurism-terminal: vterm allocation failed\n"), 1;
    vterm_set_utf8(t.vt, 1);
    t.screen = vterm_obtain_screen(t.vt);
    static const VTermScreenCallbacks callbacks = {
        .sb_pushline = history_push,
        .sb_popline = history_pop,
        .sb_clear = history_clear,
    };
    vterm_screen_set_callbacks(t.screen, &callbacks, &t);
    VTermColor fg = {.rgb = {VTERM_COLOR_RGB, 222, 235, 244}};
    VTermColor bg = {.rgb = {VTERM_COLOR_RGB, 10, 19, 29}};
    vterm_screen_set_default_colors(t.screen, &fg, &bg);
    vterm_screen_enable_altscreen(t.screen, 1);
    vterm_screen_reset(t.screen, 1);
    vterm_output_set_callback(t.vt, terminal_output, &t);
    t.window = XCreateSimpleWindow(t.display, RootWindow(t.display, t.screen_number),
                                   120, 90, (unsigned)t.width, (unsigned)t.height, 0,
                                   BlackPixel(t.display, t.screen_number),
                                   BlackPixel(t.display, t.screen_number));
    XStoreName(t.display, t.window, geteuid() == 0 ?
               "Heurism Administrator" : "Heurism C Terminal");
    XClassHint hint = {.res_name = "heurism-terminal", .res_class = "HeurismTerminal"};
    XSetClassHint(t.display, t.window, &hint);
    t.wm_delete = XInternAtom(t.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(t.display, t.window, &t.wm_delete, 1);
    XSelectInput(t.display, t.window, ExposureMask | StructureNotifyMask |
                 KeyPressMask | ButtonPressMask | FocusChangeMask);
    t.gc = XCreateGC(t.display, t.window, 0, NULL);
    t.draw = XftDrawCreate(t.display, t.window, t.visual,
                           DefaultColormap(t.display, t.screen_number));
    if (!t.draw) return fprintf(stderr, "heurism-terminal: Xft failed\n"), 1;
    XMapWindow(t.display, t.window);
    XIM xim = XOpenIM(t.display, NULL, NULL, NULL);
    XIC xic = xim ? XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                             XNClientWindow, t.window, XNFocusWindow, t.window, NULL) : NULL;
    struct winsize size = {.ws_row = (unsigned short)t.rows, .ws_col = (unsigned short)t.cols,
                           .ws_xpixel = (unsigned short)t.width, .ws_ypixel = (unsigned short)t.height};
    t.child = forkpty(&t.master, NULL, NULL, &size);
    if (t.child < 0) return perror("forkpty"), 1;
    if (t.child == 0) {
        struct passwd *account = getpwuid(getuid());
        if (!account || !account->pw_dir || chdir(account->pw_dir) < 0) {
            perror("terminal home directory");
            _exit(126);
        }
        setenv("HOME", account->pw_dir, 1);
        setenv("TERM", "xterm-256color", 1);
        setenv("SHELL", SHELL_PATH, 1);
        execl(SHELL_PATH, SHELL_PATH, (char *)NULL);
        perror("heurism-sh");
        _exit(127);
    }
    int flags = fcntl(t.master, F_GETFL);
    if (flags >= 0) fcntl(t.master, F_SETFL, flags | O_NONBLOCK);
    while (t.running) {
        while (XPending(t.display)) {
            XEvent event;
            XNextEvent(t.display, &event);
            if (event.type == Expose && !event.xexpose.count) redraw(&t);
            else if (event.type == ConfigureNotify)
                resize_terminal(&t, event.xconfigure.width, event.xconfigure.height);
            else if (event.type == KeyPress) keypress(&t, &event.xkey, xic);
            else if (event.type == ButtonPress && event.xbutton.button == Button4)
                scroll_view(&t, 3);
            else if (event.type == ButtonPress && event.xbutton.button == Button5)
                scroll_view(&t, -3);
            else if (event.type == FocusIn && xic) XSetICFocus(xic);
            else if (event.type == FocusOut && xic) XUnsetICFocus(xic);
            else if (event.type == ClientMessage &&
                     (Atom)event.xclient.data.l[0] == t.wm_delete) t.running = false;
        }
        struct pollfd fds[2] = {{.fd = ConnectionNumber(t.display), .events = POLLIN},
                                {.fd = t.master, .events = POLLIN |
                                 (t.outgoing_length ? POLLOUT : 0)}};
        int ready = poll(fds, 2, 100);
        if (ready < 0 && errno != EINTR) { perror("poll"); break; }
        if (fds[1].revents & POLLOUT) flush_outgoing(&t);
        if (fds[1].revents & (POLLIN | POLLHUP)) {
            char bytes[8192];
            ssize_t n = read(t.master, bytes, sizeof bytes);
            if (n > 0) {
                vterm_input_write(t.vt, bytes, (size_t)n);
                vterm_screen_flush_damage(t.screen);
                redraw(&t);
            } else if (n == 0 || (n < 0 && errno == EIO)) t.running = false;
        }
        int status;
        if (waitpid(t.child, &status, WNOHANG) == t.child) {
            t.child = -1;
            t.running = false;
        }
    }
    if (t.child > 0) {
        kill(-t.child, SIGHUP);
        waitpid(t.child, NULL, 0);
    }
    if (xic) XDestroyIC(xic);
    if (xim) XCloseIM(xim);
    XftDrawDestroy(t.draw);
    XftFontClose(t.display, t.font);
    XFreeGC(t.display, t.gc);
    XDestroyWindow(t.display, t.window);
    XCloseDisplay(t.display);
    history_clear(&t);
    vterm_free(t.vt);
    close(t.master);
    return 0;
}

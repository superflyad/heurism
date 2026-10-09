CC ?= cc
PKG_CONFIG ?= pkgconf
CFLAGS ?= -O2
STRICT_CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -fstack-protector-strong -D_FORTIFY_SOURCE=2
X11_CFLAGS = $(shell $(PKG_CONFIG) --cflags x11 xft)
X11_LIBS = $(shell $(PKG_CONFIG) --libs x11 xft)
TERMINAL_CFLAGS = $(X11_CFLAGS) $(shell $(PKG_CONFIG) --cflags vterm)
TERMINAL_LIBS = $(X11_LIBS) $(shell $(PKG_CONFIG) --libs vterm) -lutil
CONTROL_CFLAGS = $(shell $(PKG_CONFIG) --cflags json-c)
CONTROL_LIBS = $(shell $(PKG_CONFIG) --libs json-c)

.PHONY: all clean
all: companion-sh companion-terminal companion-control companionctl companion-desktop companion-app companion-session-config companion-release

companion-sh: companion-sh.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) -o $@ $<

companion-terminal: companion-terminal.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(TERMINAL_CFLAGS) -o $@ $< $(TERMINAL_LIBS)

companion-control: companion-control.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS) -lcrypto

companionctl: companionctl.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

companion-desktop: companion-desktop.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(X11_CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(X11_LIBS) $(CONTROL_LIBS)

companion-app: companion-app.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(X11_CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(X11_LIBS) $(CONTROL_LIBS)

companion-session-config: companion-session-config.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

companion-release: companion-release.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

clean:
	rm -f companion-sh companion-terminal companion-control companionctl companion-desktop companion-app companion-session-config companion-release

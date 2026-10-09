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
all: heurism-sh heurism-terminal heurism-control heurismctl heurism-desktop heurism-app heurism-session-config heurism-release

heurism-sh: heurism-sh.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) -o $@ $<

heurism-terminal: heurism-terminal.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(TERMINAL_CFLAGS) -o $@ $< $(TERMINAL_LIBS)

heurism-control: heurism-control.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS) -lcrypto

heurismctl: heurismctl.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

heurism-desktop: heurism-desktop.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(X11_CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(X11_LIBS) $(CONTROL_LIBS)

heurism-app: heurism-app.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(X11_CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(X11_LIBS) $(CONTROL_LIBS)

heurism-session-config: heurism-session-config.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

heurism-release: heurism-release.c
	$(CC) $(STRICT_CFLAGS) $(CFLAGS) $(CONTROL_CFLAGS) -o $@ $< $(CONTROL_LIBS)

clean:
	rm -f heurism-sh heurism-terminal heurism-control heurismctl heurism-desktop heurism-app heurism-session-config heurism-release

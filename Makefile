# mwm - minimal window manager for macOS

VERSION = 0.1

CC = clang
CFLAGS = -Wall -Wextra -Wno-unused-parameter -O2 -std=c11
LDFLAGS = -framework ApplicationServices -framework Carbon

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LAUNCHDIR = $(HOME)/Library/LaunchAgents

SRC = mwm.c
OBJ = $(SRC:.c=.o)

all: mwm

mwm: $(SRC) config.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f mwm $(OBJ)

install: mwm
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f mwm $(DESTDIR)$(BINDIR)
	chmod 755 $(DESTDIR)$(BINDIR)/mwm
	@echo "mwm installed to $(BINDIR)/mwm"
	@echo "Run 'make enable' to start at login"

enable:
	mkdir -p $(LAUNCHDIR)
	cp -f com.local.mwm.plist $(LAUNCHDIR)/
	launchctl load $(LAUNCHDIR)/com.local.mwm.plist
	@echo "mwm will now start at login"

disable:
	-launchctl unload $(LAUNCHDIR)/com.local.mwm.plist
	rm -f $(LAUNCHDIR)/com.local.mwm.plist
	@echo "mwm disabled from login"

uninstall: disable
	rm -f $(DESTDIR)$(BINDIR)/mwm

.PHONY: all clean install uninstall enable disable


# mwm - minimal window manager for macOS

VERSION = 0.1

CC = clang
CFLAGS = -Wall -Wextra -Wno-unused-parameter -O2 -std=c11
OBJCFLAGS = -Wall -Wextra -O2
LDFLAGS = -framework ApplicationServices -framework Carbon -framework Cocoa

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LAUNCHDIR = $(HOME)/Library/LaunchAgents

SRC = mwm.c cJSON.c
OBJC_SRC = statusbar.m
OBJ = mwm.o statusbar.o cJSON.o

all: mwm

mwm: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

mwm.o: mwm.c config.h statusbar.h
	$(CC) $(CFLAGS) -c -o $@ mwm.c

statusbar.o: statusbar.m statusbar.h
	$(CC) $(OBJCFLAGS) -c -o $@ statusbar.m

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c -o $@ cJSON.c

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
	launchctl bootstrap gui/$$(id -u) $(LAUNCHDIR)/com.local.mwm.plist
	@echo "mwm will now start at login"

disable:
	-launchctl bootout gui/$$(id -u)/com.local.mwm
	rm -f $(LAUNCHDIR)/com.local.mwm.plist
	@echo "mwm disabled from login"

start:
	@if pgrep -x mwm > /dev/null; then \
		echo "mwm is already running"; \
	else \
		launchctl bootstrap gui/$$(id -u) $(LAUNCHDIR)/com.local.mwm.plist 2>/dev/null || $(BINDIR)/mwm & \
		echo "mwm started"; \
	fi

stop:
	-launchctl bootout gui/$$(id -u)/com.local.mwm 2>/dev/null
	-pkill -x mwm
	@echo "mwm stopped"

restart: stop
	@sleep 1
	@$(MAKE) start

uninstall: disable
	rm -f $(DESTDIR)$(BINDIR)/mwm

.PHONY: all clean install uninstall enable disable start stop restart


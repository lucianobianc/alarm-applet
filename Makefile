CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS = -lXm -lXt -lX11 -lasound -lm

all: alarm-applet
alarm-applet: src/main.c src/alarm.c src/alarm.h src/sound.c src/sound.h src/storage.c src/storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/main.c src/alarm.c src/sound.c src/storage.c $(LDLIBS)

tests/test_alarm: tests/test_alarm.c src/alarm.c src/alarm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o $@ tests/test_alarm.c src/alarm.c

test: tests/test_alarm tests/test_storage
	TZ=UTC ./tests/test_alarm
	./tests/test_storage

clean:
	rm -f alarm-applet tests/test_alarm tests/test_sound tests/test_tray tests/test_storage tests/test_ui
.PHONY: all test clean

PREFIX ?= $(HOME)/.local
install: alarm-applet
	install -Dm755 alarm-applet "$(DESTDIR)$(PREFIX)/bin/alarm-applet"
	install -d "$(DESTDIR)$(PREFIX)/share/applications"
	sed 's|^Exec=.*|Exec="$(PREFIX)/bin/alarm-applet"|' data/alarm-applet.desktop > "$(DESTDIR)$(PREFIX)/share/applications/alarm-applet.desktop"
	chmod 644 "$(DESTDIR)$(PREFIX)/share/applications/alarm-applet.desktop"
.PHONY: install

tests/test_sound: tests/test_sound.c src/sound.c src/sound.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(LDFLAGS) -o $@ tests/test_sound.c src/sound.c -lasound -lm

test-audio: tests/test_sound
	python3 tests/test_sound.py

install-autostart: install
	mkdir -p "$(DESTDIR)$(HOME)/.config/autostart"
	sed 's|^Exec=.*|Exec="$(PREFIX)/bin/alarm-applet" --hidden|' data/alarm-applet.desktop > "$(DESTDIR)$(HOME)/.config/autostart/alarm-applet.desktop"
.PHONY: test-audio install-autostart

tests/test_tray: tests/test_tray.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< -lX11

test-tray: alarm-applet tests/test_tray
	./tests/test_tray
.PHONY: test-tray

tests/test_storage: tests/test_storage.c src/storage.c src/storage.h src/alarm.c src/alarm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o $@ tests/test_storage.c src/storage.c src/alarm.c

tests/test_ui: tests/test_ui.c src/main.c src/alarm.c src/sound.c src/storage.c src/storage.h src/alarm.h src/sound.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(LDFLAGS) -o $@ tests/test_ui.c src/alarm.c src/sound.c src/storage.c $(LDLIBS)

test-ui: tests/test_ui
	./tests/test_ui
.PHONY: test-ui

test-runtime: alarm-applet
	python3 tests/test_runtime.py
.PHONY: test-runtime

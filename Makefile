CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS = -lXm -lXt -lX11 -lasound -lm

all: alarm-applet
alarm-applet: src/main.c src/alarm.c src/alarm.h src/sound.c src/sound.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/main.c src/alarm.c src/sound.c $(LDLIBS)

tests/test_alarm: tests/test_alarm.c src/alarm.c src/alarm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o $@ tests/test_alarm.c src/alarm.c

test: tests/test_alarm
	TZ=UTC ./tests/test_alarm

clean:
	rm -f alarm-applet tests/test_alarm tests/test_sound
.PHONY: all test clean

PREFIX ?= $(HOME)/.local
install: alarm-applet
	install -Dm755 alarm-applet $(DESTDIR)$(PREFIX)/bin/alarm-applet
	install -Dm644 data/alarm-applet.desktop $(DESTDIR)$(PREFIX)/share/applications/alarm-applet.desktop
.PHONY: install

tests/test_sound: tests/test_sound.c src/sound.c src/sound.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(LDFLAGS) -o $@ tests/test_sound.c src/sound.c -lasound -lm

test-audio: tests/test_sound
	python3 tests/test_sound.py

install-autostart: install
	mkdir -p $(DESTDIR)$(HOME)/.config/autostart
	sed 's|Exec=alarm-applet --hidden|Exec=$(PREFIX)/bin/alarm-applet --hidden|' data/alarm-applet.desktop > $(DESTDIR)$(HOME)/.config/autostart/alarm-applet.desktop
.PHONY: test-audio install-autostart

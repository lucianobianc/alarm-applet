# Alarm Applet

Weekly alarms for Linux, written in C with Motif.

- Add, edit and delete alarms; select Sunday through Saturday.
- Enter times in 24-hour or 12-hour AM/PM format.
- Set the beep frequency from 20 to 20000 Hz (default: 1000 Hz).
- Set the total beep period from 1 to 3600 seconds; Stop alarm ends it early.
- Closing the window hides it. Click the tray icon to reopen it.

The app must remain running. There is no snooze or missed-alarm notification.
Alarms use local time and are saved in `$XDG_CONFIG_HOME/alarm-applet/alarms`
(default: `~/.config/alarm-applet/alarms`). The list displays 24-hour time.

## Build and run

On Debian/Ubuntu, install `build-essential libmotif-dev libxt-dev libx11-dev libasound2-dev xfonts-75dpi`.

```sh
make
make test
make test-audio
./alarm-applet
make install
```

The default install prefix is `~/.local`. X11 is required; Wayland sessions need
XWayland. The tray uses XEmbed and requires a compatible desktop tray or extension.
`--hidden` starts without mapping the window, even before the panel is running.
Launching again opens the existing window; launching with `--hidden` hides it.

The app generates a beep through the default ALSA output. No desktop
sound theme or X11 bell is required. System volume and output routing still apply.
Overlapping alarms share a tone until the latest end time.

## Start with the desktop session

Install the app and enable startup in the tray:

```sh
make install-autostart
```

You can also put `alarm-applet --hidden &` in `.xinitrc`, or use the executable's
absolute path with `--hidden` in Xfce Session and Startup. Remove the autostart
entry to disable automatic startup.

## Font size

The default font is Helvetica at 18 pixels. Override it in `~/.Xresources`, then
run `xrdb -merge ~/.Xresources` and restart the app:

```text
AlarmApplet*fontList: -*-helvetica-medium-r-normal--24-*-*-*-*-*-iso8859-1
```

## GUI and runtime checks

With `xvfb`, `xauth` and Python 3 installed:

```sh
xvfb-run -a make test-ui test-tray test-runtime
```

# Alarm Applet

Weekly alarms for Linux, written in C with Motif.

- Add, edit and delete alarms; select Sunday through Saturday.
- Enter times in 24-hour or 12-hour AM/PM format.
- Set the total beep period from 1 to 3600 seconds; Stop sound ends it early.
- Closing the window hides it. Click the tray icon to reopen it; Quit exits.

The app must remain running. There is no snooze or missed-alarm notification.
Alarms use local time and are saved in `$XDG_CONFIG_HOME/alarm-applet/alarms`
(default: `~/.config/alarm-applet/alarms`). The list displays 24-hour time.

## Build and run

On Debian/Ubuntu, install `build-essential libmotif-dev libxt-dev libx11-dev libasound2-dev`.

```sh
make
make test
make test-audio
./alarm-applet
make install
```

The default install prefix is `~/.local`. X11 is required; Wayland sessions need
XWayland. The tray uses XEmbed and requires a compatible desktop tray or extension.
Without a tray, the window remains visible at startup.

The app generates an 880 Hz beep through the default ALSA output. No desktop
sound theme or X11 bell is required. System volume and output routing still apply.

## Start with the desktop session

Install the app and enable startup in the tray:

```sh
make install-autostart
```

`--hidden` hides the window when an X11 tray becomes available. Remove
`~/.config/autostart/alarm-applet.desktop` to disable automatic startup.

"""Run a scheduled alarm through the application and capture its ALSA output."""
import array
import fcntl
import signal
import os
from pathlib import Path
import subprocess
import tempfile
import time

with tempfile.TemporaryDirectory(prefix="alarm-runtime-") as directory:
    root = Path(directory)
    config = root / "nested/config/alarm-applet"
    config.mkdir(parents=True)
    audio = root / "alarm.raw"
    alsa = root / "alsa.conf"
    alsa.write_text(
        'pcm.silent { type null }\n'
        f'pcm.!default {{ type file slave.pcm "silent" file "{audio}" format "raw" }}\n'
    )
    environment = {
        **os.environ,
        "XDG_CONFIG_HOME": str(config.parent),
        "ALSA_CONFIG_PATH": str(alsa),
    }
    # Keep both launches within the same scheduled minute.
    while time.localtime().tm_sec > 54:
        time.sleep(0.2)
    scheduled = time.localtime()
    day = (scheduled.tm_wday + 1) % 7
    records = config / "alarms"
    records.write_text(
        f"{scheduled.tm_hour} {scheduled.tm_min} {1 << day} 2 1500\n"
        f"{scheduled.tm_hour} {scheduled.tm_min} {1 << day} 1 1000\n"
    )

    def launch():
        return subprocess.Popen(
            ["./alarm-applet", "--hidden"], env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    # Do not send IPC signals to an older instance without the protocol marker.
    lock = (config / "alarms.lock").open("w+")
    fcntl.lockf(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    unexpected_signals = []
    signal.signal(signal.SIGUSR2, lambda *_: unexpected_signals.append(True))
    previous = subprocess.run(["./alarm-applet", "--hidden"], env=environment, capture_output=True, timeout=3)
    assert previous.returncode == 1 and not unexpected_signals
    lock.close()
    process = launch()
    try:
        for _ in range(50):
            assert process.poll() is None, process.communicate()
            if audio.exists() and audio.stat().st_size == 192000 and len(records.read_text().split()) == 12:
                break
            time.sleep(0.05)
        else:
            raise AssertionError("The scheduled alarm did not produce its complete audio output")
        samples = array.array("h", audio.read_bytes())
        assert max(samples) > 5000
        crossings = sum(a <= 0 < b for a, b in zip(samples[240:11759], samples[241:11760]))
        assert abs(crossings - 360) <= 2, crossings
        assert int(records.read_text().split()[5]) > 0
    finally:
        process.terminate()
        process.communicate(timeout=3)
    assert process.returncode == 0
    audio.unlink()
    process = launch()
    try:
        time.sleep(0.8)
        assert process.poll() is None
        assert time.localtime().tm_min == scheduled.tm_min, "Minute changed during the regression check"
        assert not audio.exists(), "Restarting repeated the same alarm"
    finally:
        process.terminate()
        process.communicate(timeout=3)
    assert process.returncode == 0
print("Runtime: old-instance safety, overlapping alarms, audio, firing persistence and no repeat after restart passed.")

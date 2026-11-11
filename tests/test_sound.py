"""Capture generated audio through ALSA's file output without hardware."""
import array
import os
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "beep.raw"
    config = Path(directory) / "alsa.conf"
    config.write_text(
        'pcm.silent { type null }\n'
        'pcm.!default { type file slave.pcm "silent" '
        f'file "{output}" format "raw" }}\n'
    )
    environment = {**os.environ, "ALSA_CONFIG_PATH": str(config)}
    for frequency in (1000, 1500):
        subprocess.run(["./tests/test_sound", str(frequency)], env=environment, check=True, timeout=5)
        samples = array.array("h", output.read_bytes())
        assert len(samples) == 48000, len(samples)
        assert max(samples[:12000]) > 5000
        assert min(samples[:12000]) < -5000
        assert all(sample == 0 for sample in samples[12000:24000])
        assert max(samples[24000:36000]) > 5000
        assert all(sample == 0 for sample in samples[36000:])
        crossings = sum(a <= 0 < b for a, b in zip(samples[240:11759], samples[241:11760]))
        assert abs(crossings - frequency * 0.24) <= 2, crossings
    subprocess.run(["./tests/test_sound", "--lifecycle"], env=environment, check=True, timeout=5)
    invalid = subprocess.run(["./tests/test_sound", "20001"], env=environment, timeout=5)
    assert invalid.returncode != 0
print("Audio: configurable frequency, pauses, duration and invalid frequency passed.")

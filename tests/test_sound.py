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
    subprocess.run(
        ["./tests/test_sound"],
        env={**os.environ, "ALSA_CONFIG_PATH": str(config)},
        check=True,
    )
    samples = array.array("h", output.read_bytes())
    assert len(samples) == 48000, len(samples)
    assert max(samples[:12000]) > 5000
    assert min(samples[:12000]) < -5000
    assert all(sample == 0 for sample in samples[12000:24000])
    assert max(samples[24000:36000]) > 5000
    assert all(sample == 0 for sample in samples[36000:])
    crossings = sum(a <= 0 < b for a, b in zip(samples[240:11759], samples[241:11760]))
    assert 238 <= crossings <= 242, crossings
print("Audio: 1000 Hz tone, pauses and exact one-second duration passed.")

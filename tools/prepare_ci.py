#!/usr/bin/env python3
"""Prepare a disposable checkout with public build settings and fake audio bytes.

Never run this against a configured device checkout. Existing files and symlinks
are refused; the generated audio image is a format fixture, not playable MP3.
"""
import argparse
import hashlib
from pathlib import Path

from audio_image import ROOT, build_image


def prepare(output_dir):
    output_dir = output_dir.resolve()
    if output_dir.is_relative_to(ROOT):
        raise ValueError("Fixture output must be outside the repository")
    targets = [ROOT / "firmware/esphome" / name / "secrets.yaml"
               for name in ("feasibility", "scheduler")]
    for path in [output_dir, *targets]:
        if path.exists() or path.is_symlink():
            raise ValueError(f"Refusing to replace existing path: {path}")
    settings = (ROOT / "firmware/esphome/compile-secrets.example.yaml").read_bytes()
    output_dir.mkdir(parents=True)
    # Exclusive creation also refuses a symlink created after the preflight.
    for target in targets:
        with target.open("xb") as handle:
            handle.write(settings)
    payloads = [b"".join(hashlib.sha256(f"{name}-{i}".encode()).digest()
                        for i in range(256)) for name in ("normal", "fajr")]
    image, _ = build_image(*payloads)
    (output_dir / "athan-audio.bin").write_bytes(image)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(args.output_dir)
    except (OSError, ValueError) as error:
        parser.error(str(error))

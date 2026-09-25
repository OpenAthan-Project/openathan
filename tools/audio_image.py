#!/usr/bin/env python3
"""Build an OpenAthan v1 raw audio partition without embedding media in firmware."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import tempfile

HEADER_SIZE = 4096
PARTITION_SIZE = 0x380000
PARTITION_OFFSET = 0x410000
FLASH_SIZE = 0x800000
ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "firmware/esphome/feasibility/partitions.csv"


def validate_partitions(path=PARTITIONS):
    rows = []
    for row in csv.reader(line for line in path.read_text().splitlines() if line and not line.startswith("#")):
        name, kind, subtype, offset, size, *_ = [part.strip() for part in row]
        rows.append((name, kind, subtype, int(offset, 0), int(size, 0)))
    end = 0x9000  # bootloader and partition table precede the first data partition
    for name, kind, _, offset, size in rows:
        alignment = 0x10000 if kind == "app" else 0x1000
        if offset % alignment or size % 0x1000 or size <= 0 or offset < end or offset + size > FLASH_SIZE:
            raise ValueError(f"Invalid alignment, overlap or flash boundary: {name}")
        end = offset + size
    expected = [
        ("nvs", "data", "nvs", 0x9000, 0x5000),
        ("otadata", "data", "ota", 0xE000, 0x2000),
        ("app0", "app", "ota_0", 0x10000, 0x200000),
        ("app1", "app", "ota_1", 0x210000, 0x200000),
        ("athan_audio", "data", "0x40", PARTITION_OFFSET, PARTITION_SIZE),
    ]
    if rows != expected:
        raise ValueError("Partition layout does not match the versioned feasibility storage contract")


def build_image(normal: bytes, fajr: bytes):
    tracks = []
    cursor = HEADER_SIZE
    for track_id, (name, payload) in enumerate((("normal", normal), ("fajr", fajr)), 1):
        if not payload:
            raise ValueError(f"{name} recording is empty")
        cursor = (cursor + 15) & ~15
        if len(payload) > PARTITION_SIZE - cursor:
            raise ValueError("Recordings exceed the 3.5 MiB audio partition")
        tracks.append(dict(id=track_id, name=name, offset=cursor, length=len(payload),
                           sha256=hashlib.sha256(payload).hexdigest()))
        cursor += len(payload)
    image = bytearray(b"\xff" * PARTITION_SIZE)
    struct.pack_into("<8sII", image, 0, b"OAUDIO01", cursor, 2)
    for index, (track, payload) in enumerate(zip(tracks, (normal, fajr))):
        struct.pack_into("<IIII32s", image, 16 + index * 48, track["id"], track["offset"],
                         track["length"], 1, bytes.fromhex(track["sha256"]))
        image[track["offset"]:track["offset"] + track["length"]] = payload
    image[112:144] = hashlib.sha256(image[:112]).digest()
    return bytes(image), dict(format="OAUDIO01", partition="athan_audio", partition_offset=PARTITION_OFFSET,
                             partition_size=PARTITION_SIZE, used_bytes=cursor, free_bytes=PARTITION_SIZE-cursor,
                             tracks=tracks, image_sha256=hashlib.sha256(image).hexdigest())


def read_recording(path):
    if path.suffix.lower() != ".mp3":
        raise ValueError("Input recordings must be MP3 files")
    if not 0 < path.stat().st_size <= PARTITION_SIZE - HEADER_SIZE:
        raise ValueError(f"Recording is empty or too large: {path.name}")
    return path.read_bytes()


def atomic_write(path, data):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as handle:
        temporary = Path(handle.name)
        try:
            handle.write(data)
            handle.close()
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--normal", required=True, type=Path)
    parser.add_argument("--fajr", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        destination = args.output_dir.resolve()
        if destination.is_relative_to(ROOT):
            raise ValueError("Keep generated media outside the repository")
        validate_partitions()
        image, manifest = build_image(read_recording(args.normal), read_recording(args.fajr))
        destination.mkdir(parents=True, exist_ok=True)
        atomic_write(destination / "athan-audio.bin", image)
        atomic_write(destination / "audio-manifest.json", (json.dumps(manifest, indent=2) + "\n").encode())
        print(json.dumps(manifest, indent=2))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

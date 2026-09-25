#!/usr/bin/env python3
"""Check a compiled variant against the storage budget and firmware dependency pins.

Run with the pinned ESPHome environment (PyYAML is an ESPHome dependency).
"""
import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import re
import struct
import yaml

from audio_image import PARTITIONS, PARTITION_SIZE, ROOT, validate_partitions


def inspect(build_dir, log, audio_image):
    validate_partitions()
    pins = json.loads((ROOT / "firmware/esphome/feasibility/dependencies.json").read_text())
    if version("esphome") != pins["esphome"]:
        raise ValueError("ESPHome version differs from the stable pin")
    lock = yaml.safe_load((build_dir / "dependencies.lock").read_text())["dependencies"]
    managed = {name: {key: entry[key] for key in ("version", "component_hash")}
               for name, entry in lock.items() if entry["source"]["type"] == "service"}
    if managed != pins["managed_components"] or lock["idf"]["version"] != pins["esp_idf"]:
        raise ValueError("Resolved firmware dependencies differ from the reviewed pins")
    # ESPHome converts its pinned encryption libraries into local IDF components;
    # dependencies.lock reports '*' for those, so inspect their actual manifests.
    diagnostic = pins["optional_diagnostic_libraries"]
    local = {name: entry for name, entry in lock.items()
             if entry["source"]["type"] == "local" and name != "openathan-core"}
    if local and set(local) != set(diagnostic):
        raise ValueError("Unexpected local firmware libraries")
    for name, entry in local.items():
        manifest = json.loads((Path(entry["source"]["path"]) / "library.json").read_text())
        if manifest["version"] != diagnostic[name]:
            raise ValueError(f"Diagnostic library version differs from the reviewed pin: {name}")
    if (build_dir / "partitions.csv").read_bytes() != PARTITIONS.read_bytes():
        raise ValueError("Generated build uses a different partition table")
    compile_commands = (build_dir / "build/compile_commands.json").read_text()
    if pins["xtensa_esp_elf"] not in compile_commands:
        raise ValueError("Unexpected compiler toolchain")
    output = build_dir / "build"
    app = (output / "firmware.ota.bin").read_bytes()
    factory = (output / "firmware.factory.bin").read_bytes()
    if len(app) > 0x180000:
        raise ValueError("Application exceeds the 1.5 MiB growth-budget target")
    if len(factory) >= 0x410000:
        raise ValueError("Factory image reaches shared audio storage")
    # The factory image contains exactly the same app at the first OTA slot.
    if factory[0x10000:0x10000 + len(app)] != app:
        raise ValueError("Factory and OTA application payloads differ")
    media = audio_image.read_bytes()
    if (len(media) != PARTITION_SIZE or media[:8] != b"OAUDIO01" or
            struct.unpack_from("<I", media, 12)[0] != 2 or
            hashlib.sha256(media[:112]).digest() != media[112:144]):
        raise ValueError("Invalid audio image metadata")
    for index in range(2):
        _, offset, length, _, digest = struct.unpack_from("<IIII32s", media, 16 + index * 48)
        payload = media[offset:offset + length]
        if not length or len(payload) != length or hashlib.sha256(payload).digest() != digest:
            raise ValueError("Invalid audio payload")
        if payload[:4096] in app:
            raise ValueError("Recording data found embedded in the application")
    log_text = log.read_text().replace("\r", "\n")
    ram = re.search(r"RAM:.*used (\d+) bytes from (\d+) bytes", log_text)
    flash = re.search(r"Flash:.*used (\d+) bytes from (\d+) bytes", log_text)
    if not ram or not flash or "Successfully compiled program" not in log_text:
        raise ValueError("A successful size-reporting compile log is required")
    return dict(application_bytes=len(app), linked_image_bytes=int(flash[1]),
                static_ram_bytes=int(ram[1]), application_slot_bytes=0x200000,
                slot_free_bytes=0x200000-len(app), growth_target_passed=True,
                application_sha256=hashlib.sha256(app).hexdigest(),
                audio_in_application=False, managed_components=managed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--audio-image", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.build_dir, args.log, args.audio_image), indent=2))
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

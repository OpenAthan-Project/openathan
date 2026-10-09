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
import yaml

from audio_image import ROOT, validate_partitions


def inspect_build_profile(build_dir, app):
    from release_artifacts import reject_test_material
    defines = (build_dir / "src/esphome/core/defines.h").read_text()
    qualification = "#define OPENATHAN_UPGRADE_QUALIFICATION\n" in defines
    isolated = "#define OPENATHAN_PROVISIONING_TEST_STORAGE\n" in defines
    component = build_dir / "src/esphome/components/openathan_upgrade_qualification"
    compiled = "/openathan_upgrade_qualification/qualification.cpp" in (build_dir / "build/compile_commands.json").read_text()
    if qualification:
        if not isolated:
            raise ValueError("Qualification capacity checks require isolated storage")
        if not component.is_dir() or not compiled:
            raise ValueError("Qualification build must compile its separate component")
    elif (component.exists() or compiled or
          re.search(r"^#define (?:OPENATHAN_UPGRADE_QUALIFICATION|OPENATHAN_QUALIFICATION_\w+|USE_OPENATHAN_UPGRADE_QUALIFICATION)\b", defines, re.M)):
        raise ValueError("Production/ordinary builds must exclude the qualification component and definitions")
    reject_test_material(app, allow_qualification=qualification, allow_isolated=isolated)
    return qualification, isolated


def inspect_firmware(build_dir, log, root=ROOT):
    partitions = root / "firmware/esphome/feasibility/partitions.csv"
    validate_partitions(partitions)
    pins = json.loads((root / "firmware/esphome/feasibility/dependencies.json").read_text())
    if version("esphome") != pins["esphome"]:
        raise ValueError("ESPHome version differs from the stable pin")
    lock = yaml.safe_load((build_dir / "dependencies.lock").read_text())["dependencies"]
    managed = {name: {key: entry[key] for key in ("version", "component_hash")}
               for name, entry in lock.items() if entry["source"]["type"] == "service"}
    expected_managed = dict(pins["managed_components"])
    for name, pin in pins.get("optional_managed_components", {}).items():
        if name in managed:
            expected_managed[name] = pin
    if managed != expected_managed or lock["idf"]["version"] != pins["esp_idf"]:
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
    if (build_dir / "partitions.csv").read_bytes() != partitions.read_bytes():
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
    from release_artifacts import validate_firmware_images
    qualification, isolated = inspect_build_profile(build_dir, app)
    defines = (build_dir / "src/esphome/core/defines.h").read_text()
    waveshare = '#define OPENATHAN_HARDWARE "waveshare-esp32-s3-touch-lcd-1_85c-box-v2"' in defines
    if waveshare and (not isolated or qualification or '#define OPENATHAN_UPDATES_ENABLED 0\n' not in defines):
        raise ValueError("Waveshare capacity inspection requires isolated development with updates disabled")
    flash_bytes = 0x1000000 if waveshare else 0x800000
    validate_firmware_images(factory, app, allow_qualification=qualification, allow_isolated=isolated,
                             flash_bytes=flash_bytes)
    log_text = log.read_text().replace("\r", "\n")
    ram = re.search(r"RAM:.*used (\d+) bytes from (\d+) bytes", log_text)
    flash = re.search(r"Flash:.*used (\d+) bytes from (\d+) bytes", log_text)
    if not ram or not flash or "Successfully compiled program" not in log_text:
        raise ValueError("A successful size-reporting compile log is required")
    return dict(application_bytes=len(app), linked_image_bytes=int(flash[1]),
                static_ram_bytes=int(ram[1]), application_slot_bytes=0x200000,
                slot_free_bytes=0x200000-len(app), growth_target_passed=True,
                application_sha256=hashlib.sha256(app).hexdigest(),
                application_budget_remaining_bytes=0x180000-len(app),
                managed_components=managed)


def inspect_audio(app, audio_image):
    from release_artifacts import audio_payloads
    for payload in audio_payloads(audio_image.read_bytes()):
        if payload[:4096] in app:
            raise ValueError("Recording data found embedded in the application")
    return dict(audio_in_application=False)


def inspect(build_dir, log, audio_image):
    result = inspect_firmware(build_dir, log)
    result.update(inspect_audio((build_dir / "build/firmware.ota.bin").read_bytes(), audio_image))
    return result


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

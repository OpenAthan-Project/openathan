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
    upgrade = "#define OPENATHAN_UPGRADE_QUALIFICATION\n" in defines
    quran = "#define OPENATHAN_QURAN_QUALIFICATION\n" in defines
    dua = "#define OPENATHAN_DUA_QUALIFICATION\n" in defines
    qualification = upgrade or quran or dua
    isolated = "#define OPENATHAN_PROVISIONING_TEST_STORAGE\n" in defines
    if sum((upgrade, quran, dua)) > 1:
        raise ValueError("Qualification profiles are mutually exclusive")
    if ((not upgrade and b"OPENATHAN_QUALIFICATION_V1" in app) or
            (not quran and b"OPENATHAN_QURAN_QUALIFICATION_V1" in app) or
            (not dua and b"OPENATHAN_DUA_QUALIFICATION_V1" in app)):
        raise ValueError("Qualification identity must match the build profile")
    commands = (build_dir / "build/compile_commands.json").read_text()
    for kind, enabled, family in (("upgrade", upgrade, "OPENATHAN_QUALIFICATION_"),
                                  ("quran", quran, "OPENATHAN_QURAN_QUALIFICATION_"),
                                  ("dua", dua, "OPENATHAN_DUA_QUALIFICATION_")):
        component = build_dir / f"src/esphome/components/openathan_{kind}_qualification"
        compiled = f"/openathan_{kind}_qualification/qualification.cpp" in commands
        if enabled:
            if not isolated:
                raise ValueError("Qualification capacity checks require isolated storage")
            if not component.is_dir() or not compiled:
                raise ValueError("Qualification build must compile its separate component")
        elif (component.exists() or compiled or re.search(
                rf"^#define (?:OPENATHAN_{kind.upper()}_QUALIFICATION|{family}\w+|USE_OPENATHAN_{kind.upper()}_QUALIFICATION)\b", defines, re.M)):
            raise ValueError("Production/ordinary builds must exclude the qualification component and definitions")
    if quran and ('#define OPENATHAN_UPDATES_ENABLED 0\n' not in defines or
                  b"OPENATHAN_QURAN_QUALIFICATION_V1" not in app):
        raise ValueError("Quran qualification requires disabled updates and its identity marker")
    if dua and ('#define OPENATHAN_UPDATES_ENABLED 0\n' not in defines or
                b"OPENATHAN_DUA_QUALIFICATION_V1" not in app):
        raise ValueError("Dua qualification requires disabled updates and its identity marker")
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
    if waveshare and isolated and ('#define OPENATHAN_UPGRADE_QUALIFICATION\n' in defines or '#define OPENATHAN_UPDATES_ENABLED 0\n' not in defines):
        raise ValueError("Waveshare capacity inspection requires isolated development with updates disabled")
    flash_bytes = 0x1000000 if waveshare else 0x800000
    validate_firmware_images(factory, app, allow_qualification=qualification, allow_isolated=isolated,
                             flash_bytes=flash_bytes, hardware=("waveshare-esp32-s3-touch-lcd-1_85c-box-v2" if waveshare else "atoms3r-c126-pyramid-a167"))
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
    for payload in audio_payloads(audio_image.read_bytes(), (ROOT / "release/upgrade-public-key.pem").read_bytes()):
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

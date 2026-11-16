#!/usr/bin/env python3
"""Run the production Athan replacement service against deterministic host I/O."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from audio_image import ROOT, build_image
from dua_media import BANK_START, BANK_SIZE, bank, canonical, metadata


def check(include):
    with tempfile.TemporaryDirectory(prefix="openathan-athan-host-") as temporary:
        work = Path(temporary)
        image = bytearray(build_image(b"old synthetic regular", b"old synthetic fajr")[0])
        dua = b"host-only dua fixture"
        envelope = canonical(dict(payload=canonical(metadata("v0.8.0", dua)).decode(), signature="00" * 64))
        image[BANK_START:BANK_START + BANK_SIZE] = bank(dua, envelope)
        fixture = work / "synthetic-only.bin"
        fixture.write_bytes(image)
        recordings = []
        for role, data in (("normal", b"n" * 4097), ("fajr", b"f" * 8193)):
            recordings.append(dict(id=role + "-one", role=role, name='أذان "الفجر" 🌙',
                                   bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), file=role + ".mp3",
                                   attribution="Synthetic host fixture", source="https://example.test/fixture", license="CC0-1.0"))
        unicode_catalog = work / "unicode-catalog.json"
        payload = json.dumps(dict(schema=1, kind="athan_catalog", version="v1.0.0", recordings=recordings),
                             sort_keys=True, separators=(",", ":"), ensure_ascii=True)
        unicode_catalog.write_text(json.dumps(dict(payload=payload, signature="00" * 64)))
        executable = work / "athan-runtime"
        command = [os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   '-DOPENATHAN_FIRMWARE_VERSION="v0.8.0"', '-DOPENATHAN_BUILD_COMMIT="test"',
                   '-DOPENATHAN_UPGRADE_PUBLIC_KEY="host test key"', '-DOPENATHAN_DUA_PIPELINE',
                   "-I" + str(ROOT / "tests/upgrade_stubs"), "-I" + str(ROOT / "tests/stubs"),
                   "-I" + str(include), "-I" + str(ROOT / "lib/openathan-core/include"),
                   "-I" + str(ROOT / "firmware"), "-I" + str(ROOT / "firmware/esphome/components/openathan_device"),
                   "-I" + str(ROOT / "firmware/esphome/components/openathan_audio"),
                   str(ROOT / "tests/athan_runtime_tests.cpp"),
                   str(ROOT / "firmware/esphome/components/openathan_device/athan.cpp"),
                   str(ROOT / "firmware/esphome/components/openathan_device/dua.cpp"),
                   str(ROOT / "firmware/esphome/components/openathan_audio/partition_audio.cpp"),
                   str(ROOT / "lib/openathan-core/src/dua.cpp"), "-o", str(executable)]
        if os.uname().sysname != "Darwin":
            command.append("-lcrypto")
        for flags in ([], ["-DOPENATHAN_PROVISIONING_TEST_STORAGE"], ["-DOPENATHAN_UPDATES_ENABLED=0"]):
            subprocess.run(command + flags, check=True)
            subprocess.run([str(executable), str(fixture), str(unicode_catalog)], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduinojson", type=Path, required=True)
    check(parser.parse_args().arduinojson)

#!/usr/bin/env python3
"""Build production OTA logic against deterministic host I/O adapters."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--arduinojson", required=True, type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="openathan-upgrade-runtime-") as temporary:
    output = Path(temporary) / "test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined", "-fno-sanitize-recover=all",
        '-DOPENATHAN_ROLLBACK_BOOTLOADERS="c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479"',
        '-DOPENATHAN_FIRMWARE_VERSION="v0.2.0"', '-DOPENATHAN_BUILD_COMMIT="test"', '-DOPENATHAN_UPGRADE_PUBLIC_KEY="test key"',
        "-I"+str(ROOT / "tests/upgrade_stubs"), "-I"+str(ROOT / "tests/stubs"), "-I"+str(args.arduinojson),
        "-I"+str(ROOT / "firmware/esphome/components/openathan_device"),
        str(ROOT / "tests/upgrade_runtime_tests.cpp"), str(ROOT / "firmware/esphome/components/openathan_device/upgrade.cpp"),
        *([] if os.uname().sysname == "Darwin" else ["-lcrypto"]), "-o", str(output)], check=True)
    subprocess.run([output], check=True)

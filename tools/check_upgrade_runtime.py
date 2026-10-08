#!/usr/bin/env python3
"""Build production and qualification OTA logic against deterministic host I/O."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--arduinojson", required=True, type=Path)
args = parser.parse_args()
for extra in ([], ["-DOPENATHAN_UPGRADE_QUALIFICATION"], ["-DOPENATHAN_PROVISIONING_TEST_STORAGE"]):
    rejected = subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsyntax-only", "-x", "c++",
        *extra, "-I"+str(ROOT / "tests/upgrade_stubs"), "-I"+str(ROOT / "tests/stubs"),
        "-I"+str(ROOT / "firmware"), "-I"+str(args.arduinojson), "-"],
        input='#include "esphome/components/openathan_upgrade_qualification/qualification.h"\n',
        text=True, capture_output=True)
    assert rejected.returncode != 0 and "explicit isolated qualification build" in rejected.stderr
print("Qualification component refuses production and incomplete test profiles")
with tempfile.TemporaryDirectory(prefix="openathan-upgrade-runtime-") as temporary:
    output = Path(temporary) / "test"
    # Compile the HTTP entrypoint unchanged, as in check_upgrade_http.py. Only
    # platform I/O and API dispatch are substituted; the USB gate and all access
    # checks come from production source. Fail rather than silently omit a body.
    device = (ROOT / "firmware/esphome/components/openathan_device/device.cpp").read_text()
    def function(signature):
        match = re.search(r"^" + re.escape(signature) + r" \{(?:[^\n]*\}\n|.*?^\}\n)", device, re.M | re.S)
        if not match:
            raise ValueError(f"Production HTTP function not found: {signature}")
        return match.group(0)
    (Path(temporary) / "device_http_helpers.inc").write_text("\n".join(function(signature) for signature in (
        "uint64_t now_ms()", "std::string random_nonce()",
        "void error(HttpExchange& request, int code, const char* message)")))
    (Path(temporary) / "device_http_handler.inc").write_text("\n".join(function(signature) for signature in (
        "bool Device::valid_host_(const std::string& host) const",
        "void Device::handle_http_(HttpExchange& request)")))
    def run(source, version, extra=(), additional=()):
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined", "-fno-sanitize-recover=all",
            '-DOPENATHAN_ROLLBACK_BOOTLOADERS="c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479"',
            f'-DOPENATHAN_FIRMWARE_VERSION="{version}"', '-DOPENATHAN_BUILD_COMMIT="test"', '-DOPENATHAN_UPGRADE_PUBLIC_KEY="test key"',
            *extra, "-I"+str(ROOT / "tests/upgrade_stubs"), "-I"+str(ROOT / "tests/stubs"), "-I"+str(args.arduinojson),
            "-I"+temporary,
            "-I"+str(ROOT / "firmware"),
            "-I"+str(ROOT / "firmware/esphome/components/openathan_device"), "-I"+str(ROOT / "lib/openathan-core/include"),
            str(ROOT / "tests" / source), str(ROOT / "firmware/esphome/components/openathan_device/upgrade.cpp"),
            str(ROOT / "firmware/esphome/components/openathan_device/usb_upgrade.cpp"),
            *additional,
            *([str(ROOT / "firmware/esphome/components/openathan_upgrade_qualification/qualification.cpp")]
              if "-DOPENATHAN_UPGRADE_QUALIFICATION" in extra else []),
            *([] if os.uname().sysname == "Darwin" else ["-lcrypto"]), "-o", str(output)], check=True)
        subprocess.run([output], check=True)
    run("device_http_tests.cpp", "v0.2.0", additional=(str(ROOT / "firmware/esphome/components/openathan_device/digest.cpp"),))
    for version in ("v0.2.0", "v0.2.1", "v0.3.0", "v0.4.0"):
        run("upgrade_runtime_tests.cpp", version)
    run("usb_upgrade_tests.cpp", "v0.2.0")
    run("usb_upgrade_tests.cpp", "v0.3.0")
    run("usb_upgrade_tests.cpp", "v0.4.0")
    run("upgrade_isolated_tests.cpp", "v0.2.0", ["-DOPENATHAN_PROVISIONING_TEST_STORAGE"])
    for failure in ("false", "true"):
        run("upgrade_qualification_tests.cpp", "v0.2.0", [
            "-DOPENATHAN_UPGRADE_QUALIFICATION", "-DOPENATHAN_PROVISIONING_TEST_STORAGE",
            '-DOPENATHAN_QUALIFICATION_ORIGIN="https://192.168.1.2:8443"', '-DOPENATHAN_QUALIFICATION_CA="test CA"',
            f"-DOPENATHAN_QUALIFICATION_STARTUP_FAILURE={failure}"])

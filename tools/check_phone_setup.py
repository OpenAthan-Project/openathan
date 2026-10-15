#!/usr/bin/env python3
"""Execute production phone enrollment methods with deterministic host I/O."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEVICE = ROOT / "firmware/esphome/components/openathan_device"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--arduinojson", required=True, type=Path)
args = parser.parse_args()
include = str(args.arduinojson)
device = (DEVICE / "device.cpp").read_text()
network = (DEVICE / "phone_setup_network.cpp").read_text()

def function(source, signature):
    match = re.search(r"^" + re.escape(signature) + r" \{(?:[^\n]*\}\n|.*?^\}\n)", source, re.M | re.S)
    if match is None:
        raise ValueError(f"Production function not found: {signature}")
    return match.group(0)

with tempfile.TemporaryDirectory(prefix="openathan-phone-backend-") as temporary:
    directory = Path(temporary)
    helpers = ("uint64_t now_ms()", "std::string random_nonce()", "void error(HttpExchange& request, int code, const char* message)")
    (directory / "phone_device_helpers.inc").write_text("\n".join(function(device, s) for s in helpers))
    signatures = ("void Device::restore_wifi_()", "void Device::send_(bool extension, uint8_t type, const std::vector<uint8_t>& data)",
                  "void Device::result_(bool extension, uint8_t command, const std::vector<std::string>& fields)",
                  "uint8_t Device::wifi_state_() const", "void Device::serial_request(const Frame& request)",
                  "void Device::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6>)",
                  "void Device::loop()", "bool Device::valid_host_(const std::string& host) const", "void Device::handle_http_(HttpExchange& request)")
    bodies = [function(device, s) for s in signatures]
    bodies += [function(network, s) for s in ("bool Device::phone_eligible_() const", "bool Device::phone_setup_active() const", "void Device::phone_loop_(uint64_t now)")]
    (directory / "phone_device_methods.inc").write_text("\n".join(bodies))
    # Same test-only compilation seam as check_upgrade_runtime.py: the
    # production handler is copied intact, including its RAII body scrub.
    http = (DEVICE / "phone_setup_http.cpp").read_text()
    http = http.split("namespace esphome::openathan_device {", 1)[1].rsplit("}", 1)[0]
    (directory / "phone_http.inc").write_text(http)
    for isolated in (False, True):
        output = directory / ("isolated" if isolated else "production")
        command = [os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   '-DOPENATHAN_HARDWARE="test-board"', '-DOPENATHAN_FIRMWARE_VERSION="test-version"',
                   "-I" + str(directory), "-I" + str(ROOT / "tests/upgrade_stubs"), "-I" + str(ROOT / "tests/stubs"),
                   "-I" + str(ROOT / "tests"), "-I" + str(DEVICE), "-I" + str(ROOT / "firmware"),
                   "-I" + str(ROOT / "lib/openathan-core/include"), "-I" + include]
        if isolated:
            command.append("-DOPENATHAN_PROVISIONING_TEST_STORAGE")
        command += [str(ROOT / "tests/phone_setup_backend_tests.cpp"), str(DEVICE / "credential_store.cpp"),
                    str(DEVICE / "protocol.cpp"), str(DEVICE / "digest.cpp"), str(DEVICE / "test_maintenance.cpp")]
        if os.uname().sysname != "Darwin":
            command.append("-lcrypto")
        subprocess.run(command + ["-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)

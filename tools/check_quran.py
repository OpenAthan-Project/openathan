"""Exercise the production Quran worker against deterministic HTTP/task adapters."""
import os
from pathlib import Path
import subprocess
import tempfile
import argparse

ROOT = Path(__file__).resolve().parents[1]

def check(arduinojson):
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / "quran-tests"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
            "-fno-sanitize-recover=all", "-I" + str(ROOT / "tests/upgrade_stubs"),
            "-I" + str(ROOT / "tests/quran_stubs"),
            "-I" + str(ROOT / "tests/stubs"), "-I" + arduinojson,
            "-I" + str(ROOT / "lib/openathan-core/include"),
            "-I" + str(ROOT / "firmware/esphome/components/openathan_device"),
            str(ROOT / "tests/quran_service_tests.cpp"),
            str(ROOT / "firmware/esphome/components/openathan_device/quran.cpp"),
            "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduinojson", required=True)
    check(parser.parse_args().arduinojson)

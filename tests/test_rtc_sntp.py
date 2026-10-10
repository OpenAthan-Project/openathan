"""Exercise RTC preservation with the pinned production ESPHome SNTP source."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class RtcSntpTests(unittest.TestCase):
    def test_production_sntp_startup_and_network_updates(self):
        import esphome
        package = Path(esphome.__file__).resolve().parent
        core = Path(os.environ.get("OPENATHAN_CORE_LIB", ROOT / "build/libopenathan_core.a"))
        self.assertTrue(core.is_file(), "Build openathan_core before running tests")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "rtc-sntp"
            result = subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20",
                "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DUSE_ESP32", "-DSNTP_SERVER_COUNT=3",
                "-I" + str(ROOT / "tests/rtc_stubs"), "-I" + str(ROOT / "tests/stubs"),
                "-I" + str(ROOT / "firmware"), "-I" + str(ROOT / "lib/openathan-core/include"),
                "-I" + str(package.parent), str(ROOT / "tests/rtc_sntp_tests.cpp"),
                str(ROOT / "firmware/esphome/components/waveshare_box_rtc/rtc.cpp"),
                str(package / "components/sntp/sntp_component.cpp"), str(core), "-o", str(binary)],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

"""Compile the production shutdown method with a pending HTTP-request adapter."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
@unittest.skipUnless(importlib.util.find_spec("esphome"), "Use the pinned ESPHome environment")
class DeviceShutdownTests(unittest.TestCase):
    def test_shutdown_with_pending_request(self):
        import esphome
        package = Path(esphome.__file__).resolve().parent
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "shutdown"
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-DUSE_TIME_TIMEZONE", "-I"+str(ROOT/"tests/shutdown_stubs"),
                "-I"+str(ROOT/"tests/stubs"), "-I"+str(ROOT/"lib/openathan-core/include"),
                "-I"+str(ROOT/"firmware"), "-I"+str(package.parent), "-I"+str(ROOT/"firmware/esphome/components/openathan_device"),
                str(ROOT/"tests/device_shutdown_tests.cpp"),
                str(ROOT/"firmware/esphome/components/openathan_device/shutdown.cpp"),
                "-o", str(output)], check=True)
            result = subprocess.run([str(output)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

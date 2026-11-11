"""Exercise the finder worker and verify its ESPHome registration."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
DEVICE = ROOT / "firmware/esphome/components/openathan_device"
CORE = ROOT / "lib/openathan-core"


class SpeakerDiscoveryTests(unittest.TestCase):
    def test_worker_and_shutdown(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            headers = ("esp_netif.h", "esp_random.h", "mdns.h", "lwip/sockets.h",
                       "freertos/FreeRTOS.h", "freertos/task.h", "esphome/core/defines.h",
                       "esphome/core/component.h", "esphome/core/application.h",
                       "esphome/core/hal.h", "esphome/core/log.h")
            for name in headers:
                header = root / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "speaker_discovery_platform.h"\n')

            binary = root / "speaker-discovery"
            subprocess.run([
                os.environ.get("CXX", "c++"), "-std=c++20", "-pthread", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-DUSE_OPENATHAN_SPEAKER_DISCOVERY",
                "-I", root, "-I", ROOT / "tests", "-I", DEVICE, "-I", CORE / "include",
                ROOT / "tests/speaker_discovery_tests.cpp", DEVICE / "speaker_discovery.cpp",
                DEVICE / "speaker_finder.cpp", CORE / "src/speaker_name.cpp", "-o", binary,
            ], check=True, timeout=60)
            subprocess.run([binary], check=True, timeout=15)

    @unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the ESPHome virtual environment")
    def test_configuration_gates(self):
        configurations = (
            ("openathan.yaml", "", "openathan", True, True),
            ("openathan.yaml", "mdns:\n  disabled: true\n", "openathan", False, False),
            ("provisioning/validation.yaml", "", "openathan-test", True, False),
        )
        for profile, override, node, mdns, finder in configurations:
            with self.subTest(profile=profile, override=override), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                config = root / "review.yaml"
                config.write_text(f"packages:\n  reference: !include {ROOT}/firmware/esphome/{profile}\n" + override)
                result = subprocess.run(
                    [sys.executable, "-m", "esphome", "compile", "--only-generate", config],
                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / "build")),
                    capture_output=True, text=True, timeout=60,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                generated = root / "build" / node / "src"
                defines = (generated / "esphome/core/defines.h").read_text()
                main = (generated / "main.cpp").read_text()
                manifest = (generated / "idf_component.yml").read_text()
                self.assertEqual("#define USE_MDNS\n" in defines, mdns)
                self.assertEqual("espressif/mdns:" in manifest, mdns)
                self.assertEqual("#define USE_OPENATHAN_SPEAKER_DISCOVERY\n" in defines, finder)
                self.assertEqual("->get_speaker_discovery()" in main, finder)
                self.assertEqual("->set_asset(6," in main, finder)
                count = int(re.search(r"#define ESPHOME_COMPONENT_COUNT (\d+)", defines)[1])
                self.assertEqual(main.count("App.register_component_("), count)

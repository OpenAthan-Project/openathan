"""Check provisioning codegen and the installed ESPHome scan policy together."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class WifiScanTests(unittest.TestCase):
    def test_provisioning_generates_full_scan_results(self):
        for config, name in (("openathan.yaml", "openathan"),
                             ("provisioning/validation.yaml", "openathan-test")):
            with self.subTest(config=config), tempfile.TemporaryDirectory() as directory:
                directory = Path(directory)
                path = directory / "test.yaml"
                path.write_text(f"packages:\n  reference: !include {ROOT}/firmware/esphome/{config}\n")
                env = dict(os.environ, ESPHOME_BUILD_PATH=str(directory / "build"))
                result = subprocess.run([sys.executable, "-m", "esphome", "compile",
                                         str(path), "--only-generate"],
                                        env=env, capture_output=True, text=True, timeout=120)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                generated = (directory / "build" / name / "src/main.cpp").read_text()
                self.assertTrue("wifi::global_wifi_component->set_keep_scan_results(true);" in generated,
                                "Connected USB recovery must request full scan results")

    def test_pinned_connected_and_disconnected_scan_policy(self):
        import esphome
        package = Path(esphome.__file__).resolve().parent / "components/wifi"
        functions = []
        for filename, signature in (
            ("wifi_component.cpp", "bool WiFiComponent::needs_full_scan_results_() const"),
            ("wifi_component_esp_idf.cpp", "bool WiFiComponent::wifi_scan_start_(bool passive)"),
        ):
            source = (package / filename).read_text()
            start = source.index(signature + " {")
            # These class methods end with a closing brace at column zero.
            end = source.index("\n}", start) + 2
            functions.append(source[start:end])
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            (directory / "wifi_scan_functions.inc").write_text("\n".join(functions))
            binary = directory / "wifi-scan-probe"
            result = subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20",
                "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I" + str(directory),
                str(ROOT / "tests/wifi_scan_probe.cpp"), "-o", str(binary)],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

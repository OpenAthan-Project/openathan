"""Exercise the pinned display code generator, not just the presenter fake LCD."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class DisplayConfigTests(unittest.TestCase):
    def test_reference_writer_and_brightness_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "display.yaml"
            reference = ROOT / "firmware/esphome/openathan.yaml"
            base = f"packages:\n  reference: !include {reference}\n"
            config.write_text(base)
            build = root / "build"
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(build)), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            source = (build / "openathan/src/main.cpp").read_text()
            self.assertIn("MipiSpiBuffer<uint8_t", source)
            self.assertIn("athan_status_screen->draw(it)", source)
            self.assertNotIn("atom_status_lcd->show_test_card()", source)
            self.assertIn("->set_brightness(10)", source)
            for invalid in (0, 101):
                config.write_text(base + f'substitutions:\n  display_brightness_percent: "{invalid}"\n')
                result = subprocess.run([sys.executable, "-m", "esphome", "config", str(config)],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

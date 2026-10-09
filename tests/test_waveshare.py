"""Development capacity and profile guards never widen the public release contract."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from release_fixtures import firmware

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from release_artifacts import validate_firmware_images


class WaveshareCapacityTests(unittest.TestCase):
    def test_explicit_16mb_headers_and_release_restriction(self):
        factory, app = firmware(0x1000000)
        with self.assertRaisesRegex(ValueError, "8 MiB"):
            validate_firmware_images(factory, app)
        with self.assertRaisesRegex(ValueError, "isolated"):
            validate_firmware_images(factory, app, flash_bytes=0x1000000)
        self.assertEqual(validate_firmware_images(factory, app, allow_isolated=True, flash_bytes=0x1000000), app)
        old_factory, old_app = firmware()
        with self.assertRaisesRegex(ValueError, "16 MiB"):
            validate_firmware_images(old_factory, old_app, allow_isolated=True, flash_bytes=0x1000000)

    def test_generated_board_and_profile_guards(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "waveshare.yaml"
            base = f"packages:\n  waveshare: !include {ROOT / 'firmware/esphome/waveshare/development.yaml'}\n"
            config.write_text(base)
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / "build")), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            build = root / "build/openathan-test"
            defines = (build / "src/esphome/core/defines.h").read_text()
            self.assertIn('#define OPENATHAN_UPDATES_ENABLED 0\n', defines)
            self.assertIn('#define OPENATHAN_PROVISIONING_TEST_STORAGE\n', defines)
            source = (build / "src/main.cpp").read_text()
            self.assertIn('MipiSpiBuffer<uint16_t', source)
            self.assertIn('->set_round(true)', source)
            self.assertIn('waveshare_status_lcd->set_rotation(display::DISPLAY_ROTATION_180_DEGREES)', source)
            self.assertIn('athan_scheduler->set_playback(waveshare_audio)', source)
            self.assertIn('power_save_mode: NONE', source)
            self.assertNotIn('->set_power_save_mode(', source)
            self.assertNotIn('front_button', source)
            self.assertNotIn('pyramid_', source)
            for override in ('openathan_device:\n  updates_enabled: true\n',
                             'esphome:\n  name: openathan\n',
                             'logger:\n  level: DEBUG\n',
                             'logger:\n  level: DEBUG\n  baud_rate: 0\n',
                             'substitutions:\n  display_brightness_percent: "0"\n'):
                config.write_text(base + override)
                result = subprocess.run([sys.executable, "-m", "esphome", "config", str(config)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            config.write_text(base + 'logger:\n  level: DEBUG\n  baud_rate: 0\napi:\n  encryption:\n    key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=\n')
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / "build")), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('#define OPENATHAN_USB_OWNS_DRIVER\n', (build / 'src/esphome/core/defines.h').read_text())

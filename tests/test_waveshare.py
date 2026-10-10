"""Development capacity and profile guards never widen the public release contract."""
import os
from pathlib import Path
import re
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
        with self.assertRaisesRegex(ValueError, "Hardware"):
            validate_firmware_images(factory, app, flash_bytes=0x1000000)
        self.assertEqual(validate_firmware_images(factory, app, allow_isolated=True, flash_bytes=0x1000000, hardware="waveshare-esp32-s3-touch-lcd-1_85c-box-v2"), app)
        old_factory, old_app = firmware()
        with self.assertRaisesRegex(ValueError, "16 MiB"):
            validate_firmware_images(old_factory, old_app, allow_isolated=True, flash_bytes=0x1000000, hardware="waveshare-esp32-s3-touch-lcd-1_85c-box-v2")

    def test_production_profile_uses_production_storage_and_updates(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary) / "build"
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate",
                str(ROOT / "firmware/esphome/waveshare/production.yaml")],
                env=dict(os.environ, ESPHOME_BUILD_PATH=str(build)), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            defines = (build / "openathan/src/esphome/core/defines.h").read_text()
            self.assertIn('#define OPENATHAN_UPDATES_ENABLED 1\n', defines)
            self.assertIn('"waveshare-box-v2.upgrade.json"', defines)
            self.assertNotIn('OPENATHAN_PROVISIONING_TEST_STORAGE', defines)
            self.assertNotIn('OPENATHAN_UPGRADE_QUALIFICATION', defines)
            self.assertIn('waveshare_rtc->set_network_time(athan_clock)', (build / 'openathan/src/main.cpp').read_text())

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
            self.assertIn('athan_status_screen->set_stop_button(true)', source)
            self.assertIn('waveshare_status_lcd->set_rotation(display::DISPLAY_ROTATION_180_DEGREES)', source)
            self.assertIn('athan_scheduler->set_playback(waveshare_audio)', source)
            self.assertIn('waveshare_rtc->set_network_time(athan_clock)', source)
            self.assertIn('waveshare_rtc->set_i2c_bus(waveshare_bus)', source)
            self.assertIn('waveshare_rtc->set_i2c_address(0x51)', source)
            self.assertIn('waveshare_rtc->set_update_interval(4294967295UL)', source)
            self.assertIn('waveshare_box_rtc::RTC()', source)
            self.assertIn('power_save_mode: NONE', source)
            self.assertNotIn('->set_power_save_mode(', source)
            self.assertNotIn('front_button', source)
            self.assertNotIn('pyramid_', source)
            # Check the generated input/action wiring, not just the YAML text.
            pin_match = re.search(r'boot_button->set_pin\((\w+)\)', source)
            self.assertIsNotNone(pin_match, 'BOOT input must be connected to a GPIO pin')
            pin = pin_match.group(1)
            self.assertIn(f'{pin}->set_pin(::GPIO_NUM_0)', source)
            self.assertIn(f'{pin}->set_inverted(true)', source)
            self.assertIn(f'{pin}->set_flags((gpio::Flags::FLAG_INPUT | gpio::Flags::FLAG_PULLUP))', source)
            self.assertIn('boot_button->set_trigger_on_initial_state(false)', source)
            clicks = re.findall(r'ClickTrigger\(boot_button, (\d+), (\d+)\);.*?'
                                r'ControlAction<>\(athan_scheduler, (\d+)\);', source, re.S)
            self.assertEqual(clicks, [('50', '1000', '0'), ('2000', '5000', '1'), ('6000', '10000', '2')])
            for override in ('openathan_device:\n  updates_enabled: true\n',
                             'esphome:\n  name: openathan\n',
                             'logger:\n  level: DEBUG\n',
                             'logger:\n  level: DEBUG\n  baud_rate: 0\n',
                             'substitutions:\n  display_brightness_percent: "0"\n'):
                config.write_text(base + override)
                result = subprocess.run([sys.executable, "-m", "esphome", "config", str(config)], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            config.write_text(base + 'logger:\n  level: DEBUG\n  baud_rate: 0\napi:\n  encryption:\n    key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=\nopenathan_display:\n  stop_button: false\n')
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / "build")), capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('#define OPENATHAN_USB_OWNS_DRIVER\n', (build / 'src/esphome/core/defines.h').read_text())
            self.assertIn('athan_status_screen->set_stop_button(false)', (build / 'src/main.cpp').read_text())

    def test_volume_routes_for_each_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Included diagnostic profiles fall back to the main config's secrets.
            (root / 'secrets.yaml').write_text('diagnostic_api_key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=\n')
            for profile in ('waveshare/audio', 'waveshare/development',
                            'waveshare/diagnostics', 'openathan'):
                with self.subTest(profile=profile):
                    config = root / 'volume.yaml'
                    config.write_text(f"packages:\n  product: !include {ROOT / 'firmware/esphome' / (profile + '.yaml')}\n")
                    build = root / profile.replace('/', '-')
                    result = subprocess.run([sys.executable, '-m', 'esphome', 'compile',
                                             '--only-generate', str(config)],
                                            env=dict(os.environ, ESPHOME_BUILD_PATH=str(build)),
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    name = 'openathan-test' if profile.startswith('waveshare/') else 'openathan'
                    source = (build / name / 'src/main.cpp').read_text()
                    if profile.startswith('waveshare/'):
                        self.assertIn('athan_player->set_volume_max(1.0f)', source)
                        self.assertIn('waveshare_audio->set_dac(waveshare_dac)', source)
                        self.assertNotIn('waveshare_speaker->set_audio_dac(', source)
                    else:
                        self.assertIn('athan_player->set_volume_max(0.6f)', source)
                        self.assertIn('pyramid_speaker->set_audio_dac(pyramid_dac)', source)

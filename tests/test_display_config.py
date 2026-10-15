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
    def test_reference_writer_rotation_and_brightness_bounds(self):
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
            # Pinned ESPHome omits the setter for its default 0-degree rotation.
            # Any nonzero rotation emits a setter, including the old 180 value.
            self.assertNotIn("atom_status_lcd->set_rotation(", source)
            self.assertIn("athan_status_screen->draw(it)", source)
            self.assertNotIn("atom_status_lcd->show_test_card()", source)
            self.assertIn("->set_brightness(50)", source)
            self.assertIn("athan_scheduler->set_display_output(atom_backlight)", source)
            self.assertIn("athan_status_screen->set_connection_reader(", source)
            for accessor in ("hostname", "ipv4_address", "connected", "playback_busy"):
                self.assertIn(f"athan_device->{accessor}()", source)
            for accessor in ("active", "connecting", "error", "ssid", "ip"):
                self.assertIn(f"athan_device->phone_setup_{accessor}()", source)
            for invalid in (0, 101):
                config.write_text(base + f'substitutions:\n  display_brightness_percent: "{invalid}"\n')
                result = subprocess.run([sys.executable, "-m", "esphome", "config", str(config)],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_connection_reader_is_optional_and_shared_by_round_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "display.yaml"
            for profile, override, expect_reader in (
                ("openathan.yaml", "openathan_display:\n  device_id: !remove\n", False),
                ("waveshare/production.yaml", "", True),
            ):
                reference = ROOT / "firmware/esphome" / profile
                config.write_text(f"packages:\n  reference: !include {reference}\n" + override)
                build = root / ("round" if expect_reader else "optional")
                result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                        env=dict(os.environ, ESPHOME_BUILD_PATH=str(build)), capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                source = (build / "openathan/src/main.cpp").read_text()
                self.assertEqual("athan_status_screen->set_connection_reader(" in source, expect_reader)
                if expect_reader:
                    self.assertIn("athan_status_screen->set_round(true)", source)
                else:
                    self.assertNotIn("openathan_device", (build / "openathan/src/esphome/components/openathan_display/status_display.h").read_text())

    def test_board_display_composes_without_product_device(self):
        profiles = (
            ("voice-pyramid", "atom-s3r-gc9107-display", "pyramid_speaker", "athan_audio"),
            ("waveshare-box-v2", "waveshare-box-v2-display", "waveshare_speaker", "waveshare_audio"),
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for hardware, screen, speaker, playback in profiles:
                with self.subTest(board=hardware):
                    config = root / "standalone.yaml"
                    config.write_text(f"""
esphome:
  name: standalone-display
packages:
  scheduler: !include {ROOT}/firmware/esphome/packages/openathan.yaml
  hardware: !include {ROOT}/firmware/esphome/packages/hardware/{hardware}.yaml
  screen: !include {ROOT}/firmware/esphome/boards/{screen}.yaml
external_components:
  - source:
      type: local
      path: {ROOT}/firmware/esphome/components
    components: [openathan_audio]
logger:
wifi:
  ssid: compile-only
  password: compile-only
time:
  - platform: sntp
    id: athan_clock
    timezone: UTC
media_player:
  - platform: speaker
    id: athan_player
    announcement_pipeline:
      speaker: {speaker}
      format: MP3
      sample_rate: 44100
      num_channels: 1
openathan_audio:
  id: athan_audio
  media_player_id: athan_player
openathan:
  id: athan_scheduler
  time_id: athan_clock
  playback_id: {playback}
  latitude: 0
  longitude: 0
  method: muslim_world_league
""")
                    build = root / hardware
                    result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                            env=dict(os.environ, ESPHOME_BUILD_PATH=str(build)), capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    source = (build / "standalone-display/src/main.cpp").read_text()
                    self.assertIn("athan_status_screen->set_openathan(athan_scheduler)", source)
                    self.assertNotIn("set_connection_reader(", source)
                    self.assertNotIn("openathan_device::Device", source)
                    self.assertFalse((build / "standalone-display/src/esphome/components/openathan_device/device.cpp").exists())

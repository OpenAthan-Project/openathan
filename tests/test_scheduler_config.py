"""Validate the real ESPHome schema, when run in the pinned build environment."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the ESPHome virtual environment")
class SchedulerConfigTests(unittest.TestCase):
    def config(self, override="", timezone="timezone: America/Toronto"):
        return f"""
esphome:
  name: scheduler-schema-test
packages:
  hardware: !include {ROOT}/firmware/esphome/packages/hardware/voice-pyramid.yaml
external_components:
  - source:
      type: local
      path: {ROOT}/firmware/esphome/components
    components: [openathan, openathan_audio]
logger:
wifi:
  ssid: compile-only
  password: compile-only
time:
  - platform: sntp
    id: athan_test_clock
    {timezone}
media_player:
  - platform: speaker
    id: athan_test_player
    announcement_pipeline:
      speaker: pyramid_speaker
      format: MP3
      sample_rate: 44100
      num_channels: 1
openathan_audio:
  id: athan_test_audio
  media_player_id: athan_test_player
openathan:
  id: athan_test_scheduler
  time_id: athan_test_clock
  playback_id: athan_test_audio
  latitude: 43.6532
  longitude: -79.3832
  method: north_america
{override}
"""

    def validate(self, config, success, message=""):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.yaml"
            path.write_text(config)
            result = subprocess.run([sys.executable, "-m", "esphome", "config", str(path)],
                                    capture_output=True, text=True)
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode == 0, success, output)
            if message:
                self.assertIn(message, output)

    def test_valid_and_actions(self):
        config = self.config() + """
interval:
  - interval: 10s
    then:
      - openathan.stop:
          id: athan_test_scheduler
      - openathan.skip_next:
          id: athan_test_scheduler
      - openathan.cancel_skip:
          id: athan_test_scheduler
"""
        self.validate(config, True)

    def test_explicit_timezone_required(self):
        self.validate(self.config(timezone=""), False, "explicit timezone")

    def test_invalid_inputs(self):
        self.validate(self.config("  offsets:\n    fajr: 121"), False)
        self.validate(self.config().replace("latitude: 43.6532", "latitude: .nan"), False, "finite")
        self.validate(self.config().replace("method: north_america", "method: invented"), False)
        self.validate(self.config().replace("playback_id: athan_test_audio", "playback_id: athan_test_player"), False)

    def test_conflicting_clock_timezones(self):
        config = self.config().replace("media_player:\n", """  - platform: ds1307
    id: another_time_source
    i2c_id: pyramid_bus
    timezone: UTC
media_player:
""")
        self.validate(config, False, "same explicit timezone")

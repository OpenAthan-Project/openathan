"""Source-generation and profile boundaries for the screenless board."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AudioBoardProfiles(unittest.TestCase):
    def test_generated_profiles(self):
        for mode in ('production','development'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                env=dict(os.environ, ESPHOME_BUILD_PATH=temporary)
                config=ROOT / f'firmware/esphome/waveshare-audio-board/{mode}.yaml'
                run=subprocess.run([sys.executable,'-m','esphome','compile','--only-generate',str(config)],env=env,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                build=Path(temporary)/('openathan' if mode=='production' else 'openathan-test')
                defines=(build/'src/esphome/core/defines.h').read_text()
                source=(build/'src/main.cpp').read_text()
                self.assertIn('"waveshare-esp32-s3-audio-board"',defines)
                self.assertIn('"waveshare-audio-board.upgrade.json"',defines)
                self.assertIn(f'#define OPENATHAN_UPDATES_ENABLED {int(mode=="production")}\n',defines)
                self.assertEqual('OPENATHAN_PROVISIONING_TEST_STORAGE' in defines,mode=='development')
                for pin in ('set_scl_pin(10)','set_sda_pin(11)','set_mclk_pin(12)','set_bclk_pin(13)','set_lrclk_pin(14)','set_dout_pin(16)','set_pin(38)','GPIO_NUM_0'):
                    self.assertIn(pin,source)
                for fragment in ('set_num_leds(7)','set_light_output(audio_board_ring)','set_playback(audio_board_audio)','set_network_time(athan_clock)','adjust_volume(key == 0 ? -5 : 5)'):
                    self.assertIn(fragment,source)
                self.assertNotIn('waveshare_box_display',source)
                self.assertNotIn('openathan_display::',source)
                self.assertNotIn('pyramid_',source)
    def test_isolated_updates_cannot_be_enabled(self):
        with tempfile.TemporaryDirectory() as temporary:
            config=Path(temporary)/'invalid.yaml'
            config.write_text(f'packages:\n  board: !include {ROOT / "firmware/esphome/waveshare-audio-board/development.yaml"}\nopenathan_device:\n  updates_enabled: true\n')
            run=subprocess.run([sys.executable,'-m','esphome','config',str(config)],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertIn('updates_enabled false',run.stdout+run.stderr)

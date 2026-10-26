"""Source-generation and profile boundaries for the screenless board."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AudioBoardProfiles(unittest.TestCase):
    def test_private_upgrade_profile_and_failure_identity(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        from upgrade_qualification import initialize
        import yaml
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            trust = initialize(root / 'private-feed', '192.168.1.2',
                               hardware='waveshare-esp32-s3-audio-board')
            (root / 'secrets.yaml').write_text(yaml.safe_dump({
                **yaml.safe_load((ROOT / 'firmware/esphome/compile-secrets.example.yaml').read_text()),
                **{'qualification_' + key: value for key, value in trust.items()}}))
            config = root / 'qualification.yaml'
            base = f'packages:\n  board: !include {ROOT / "firmware/esphome/waveshare-audio-board/upgrade-qualification.yaml"}\n'
            config.write_text(base)
            for version, failure in (('v0.0.1', 'false'), ('v0.0.3', 'true')):
                result = subprocess.run([sys.executable, '-m', 'esphome',
                    '-s', 'qualification_version', version,
                    '-s', 'qualification_startup_failure', failure,
                    'compile', '--only-generate', str(config)],
                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / version)), capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                defines = (root / version / 'openathan-test/src/esphome/core/defines.h').read_text()
                for value in ('#define OPENATHAN_UPGRADE_QUALIFICATION\n',
                              '#define OPENATHAN_PROVISIONING_TEST_STORAGE\n',
                              '#define OPENATHAN_UPDATES_ENABLED 1\n',
                              '"waveshare-esp32-s3-audio-board"',
                              '"waveshare-audio-board.upgrade.json"'):
                    self.assertIn(value, defines)
                self.assertIn(f'#define OPENATHAN_QUALIFICATION_STARTUP_FAILURE {failure}\n', defines)
            config.write_text(base + 'esphome:\n  name: openathan\n')
            result = subprocess.run([sys.executable, '-m', 'esphome', 'config', str(config)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)

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

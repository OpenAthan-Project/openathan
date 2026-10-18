"""Keep Quran changes limited to the intended pinned ESPHome source files."""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class QuranVendoringTests(unittest.TestCase):
    def test_pinned_unmodified_audio_sources(self):
        components = ROOT / "firmware/esphome/components"
        manifest = json.loads((components / "audio/upstream.json").read_text())
        self.assertEqual(manifest["version"], "2026.9.0")
        self.assertIn("esphome==" + manifest["version"], (ROOT / "firmware/esphome/requirements.txt").read_text())
        modified = {"audio/audio_reader.cpp", "audio/audio_decoder.cpp", "audio/audio_decoder.h",
                    "speaker/media_player/speaker_media_player.h", "speaker/media_player/audio_pipeline.h",
                    "speaker/media_player/audio_pipeline.cpp", "speaker/media_player/__init__.py"}
        for name, expected in manifest["sha256"].items():
            if name not in modified:
                self.assertEqual(hashlib.sha256((components / name).read_bytes()).hexdigest(), expected, name)

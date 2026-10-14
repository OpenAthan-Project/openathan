"""Verify vendored provenance and calibrated-only routing using the real schema."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "third_party/esphome-i2s-audio"


class ProvenanceTests(unittest.TestCase):
    def test_patch_reconstructs_every_upstream_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(VENDOR / "components", root / "components")
            result = subprocess.run(["patch", "--batch", "--reverse", "-p1", "-i", str(VENDOR / "volume-fence.patch")],
                                    cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            hashes = json.loads((VENDOR / "upstream-sha256.json").read_text())
            paths = {str(p.relative_to(root / "components")): p for p in (root / "components").rglob("*")
                     if p.is_file() and p.suffix in (".py", ".h", ".cpp")}
            self.assertEqual(set(paths), set(hashes))
            for name, digest in hashes.items():
                self.assertEqual(hashlib.sha256(paths[name].read_bytes()).hexdigest(), digest, name)
            result = subprocess.run(["patch", "--batch", "--forward", "-p1", "-i", str(VENDOR / "volume-fence.patch")],
                                    cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            expected = {str(p.relative_to(VENDOR / "components")): hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in (VENDOR / "components").rglob("*")
                        if p.is_file() and p.suffix in (".py", ".h", ".cpp")}
            rebuilt = {str(p.relative_to(root / "components")): hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (root / "components").rglob("*")
                       if p.is_file() and p.suffix in (".py", ".h", ".cpp")}
            self.assertEqual(rebuilt, expected)
            metadata = json.loads((VENDOR / "upstream.json").read_text())
            self.assertEqual(metadata["version"], "2026.9.0")
            self.assertEqual(metadata["commit"], "c6e4c87e525dd343e470d8dea368957a002f6505")
            self.assertEqual(hashlib.sha256((VENDOR / "LICENSE").read_bytes()).hexdigest(), metadata["license_sha256"])
            license_text = (VENDOR / "LICENSE").read_text()
            self.assertIn("Copyright (c) 2019 ESPHome", license_text)
            self.assertIn("## MIT License", license_text)
            self.assertIn("## GPLv3 License", license_text)


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class FenceConfigTests(unittest.TestCase):
    def generate(self, extra="", success=True, message=""):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "secrets.yaml").write_bytes((ROOT / "firmware/esphome/compile-secrets.example.yaml").read_bytes())
            path = root / "test.yaml"
            path.write_text(f"packages:\n  profile: !include {ROOT}/firmware/esphome/scheduler/validation.yaml\n" + extra)
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(path)],
                                    env=dict(os.environ, ESPHOME_BUILD_PATH=str(root / "build")),
                                    capture_output=True, text=True)
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode == 0, success, output)
            if message:
                self.assertIn(message, output)
            if success:
                source = (root / "build/openathan-feasibility/src/main.cpp").read_text()
                defines = (root / "build/openathan-feasibility/src/esphome/core/defines.h").read_text()
                self.assertIn("pyramid_speaker->set_volume_fence(true)", source)
                self.assertIn("pyramid_audio->set_speaker(pyramid_speaker)", source)
                self.assertIn("pyramid_amplifier->set_enable_on_startup(false)", source)
                self.assertIn("USE_OPENATHAN_VOLUME_FENCE", defines)
                self.assertNotIn("pyramid_speaker_enable", source)
                self.assertNotIn("pyramid_speaker->set_audio_dac", source)

    def test_calibrated_routing(self):
        self.generate()

    def test_fence_required(self):
        self.generate("speaker:\n  - id: !extend pyramid_speaker\n    volume_fence: false\n", False, "requires volume_fence")

    def test_startup_gate_required(self):
        self.generate("voice_pyramid:\n  amplifier:\n    enable_on_startup: true\n", False, "enable_on_startup: false")

    def test_direct_dac_rejected(self):
        self.generate("speaker:\n  - id: !extend pyramid_speaker\n    audio_dac: pyramid_dac\n", False, "volume_fence requires standard I2S")

    def test_spdif_rejected(self):
        self.generate("speaker:\n  - id: !extend pyramid_speaker\n    spdif_mode: true\n    channel: stereo\n    use_apll: true\n    i2s_mode: primary\n", False, "volume_fence requires standard I2S")

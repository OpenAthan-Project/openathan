"""Regression checks for publication-time fixtures and dependency enforcement."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import audio_image
import check_feasibility
import prepare_ci


class CiFixtureTests(unittest.TestCase):
    def test_refuses_existing_secrets_and_symlinks_without_overwriting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "repo"
            target = root / "firmware/esphome/feasibility/secrets.yaml"
            target.parent.mkdir(parents=True)
            private = Path(directory) / "private.yaml"
            private.write_text("private configuration sentinel")
            with patch.object(prepare_ci, "ROOT", root):
                for symlink in (False, True):
                    if symlink:
                        target.symlink_to(private)
                    else:
                        target.write_text(private.read_text())
                    with self.assertRaises(ValueError):
                        prepare_ci.prepare(Path(directory) / "output")
                    self.assertEqual(target.read_text(), "private configuration sentinel")
                    self.assertFalse((Path(directory) / "output").exists())
                    target.unlink()


class CapacityChecksTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "firmware"
        (self.build / "build").mkdir(parents=True)
        self.pins = json.loads((ROOT / "firmware/esphome/feasibility/dependencies.json").read_text())
        self.lock = {name: {**entry, "source": {"type": "service"}}
                     for name, entry in self.pins["managed_components"].items()}
        self.lock["idf"] = {"version": self.pins["esp_idf"], "source": {"type": "idf"}}
        (self.build / "partitions.csv").write_bytes(audio_image.PARTITIONS.read_bytes())
        (self.build / "build/compile_commands.json").write_text(self.pins["xtensa_esp_elf"])
        app = b"test-application" * 100
        (self.build / "build/firmware.ota.bin").write_bytes(app)
        (self.build / "build/firmware.factory.bin").write_bytes(b"\xff" * 0x10000 + app)
        self.log = self.root / "compile.log"
        self.log.write_text("RAM: used 100 bytes from 1000 bytes\n"
                            "Flash: used 1500 bytes from 2000000 bytes\nSuccessfully compiled program")
        self.audio = self.root / "audio.bin"
        self.audio.write_bytes(audio_image.build_image(b"normal-fixture", b"fajr-fixture")[0])

    def inspect(self):
        (self.build / "dependencies.lock").write_text(yaml.safe_dump({"dependencies": self.lock}))
        with patch.object(check_feasibility, "version", return_value=self.pins["esphome"]):
            return check_feasibility.inspect(self.build, self.log, self.audio)

    def test_plain_and_encrypted_build_pins(self):
        self.assertTrue(self.inspect()["growth_target_passed"])
        for name, version in self.pins["optional_diagnostic_libraries"].items():
            path = self.root / name
            path.mkdir(parents=True)
            (path / "library.json").write_text(json.dumps({"version": version}))
            self.lock[name] = {"version": "*", "source": {"type": "local", "path": str(path)}}
        self.assertTrue(self.inspect()["growth_target_passed"])
        (self.root / "esphome/noise-c/library.json").write_text('{"version":"unexpected"}')
        with self.assertRaisesRegex(ValueError, "Diagnostic library version"):
            self.inspect()

    def test_dependency_drift(self):
        self.lock["idf"]["version"] = "unexpected"
        with self.assertRaisesRegex(ValueError, "dependencies differ"):
            self.inspect()
        self.lock["idf"]["version"] = self.pins["esp_idf"]
        self.lock["unknown"] = {"source": {"type": "local"}}
        with self.assertRaisesRegex(ValueError, "Unexpected local"):
            self.inspect()

    def test_settings_json_dependency_is_optional_but_pinned(self):
        name = "bblanchon/arduinojson"
        self.lock[name] = {**self.pins["optional_managed_components"][name], "source": {"type": "service"}}
        self.assertTrue(self.inspect()["growth_target_passed"])
        self.lock[name]["component_hash"] = "changed"
        with self.assertRaisesRegex(ValueError, "dependencies differ"):
            self.inspect()

    def test_factory_mismatch_and_app_budget(self):
        (self.build / "build/firmware.factory.bin").write_bytes(b"wrong")
        with self.assertRaisesRegex(ValueError, "payloads differ"):
            self.inspect()
        (self.build / "build/firmware.ota.bin").write_bytes(b"x" * (0x180000 + 1))
        with self.assertRaisesRegex(ValueError, "growth-budget"):
            self.inspect()

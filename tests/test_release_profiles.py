"""Board-bound release assets, using non-executable fixtures only."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import release_artifacts as artifacts
import upgrade_artifacts as upgrade
from audio_image import build_image
from release_fixtures import firmware, esp_image


class HardwareReleaseTests(unittest.TestCase):
    def bundle(self, hardware, commit='a'*40):
        profile = artifacts.profile(hardware)
        factory, _ = firmware(profile['flash_bytes'])
        payload = bytearray(256)
        struct.pack_into('<I', payload, 0, 0xabcd5432)
        payload[16:22] = b'v0.5.0'
        app = esp_image(payload, flash_bytes=profile['flash_bytes'])
        factory = factory[:0x10000] + app
        audio, _ = build_image(b'ID3-test-normal', b'ID3-test-fajr')
        manifest = artifacts.make_manifest(commit, 'v0.5.0', factory, audio, hardware)
        report = dict(schema=1, commit=commit, configuration=profile['configuration'], capacity=dict(
            application_bytes=len(app), application_sha256=artifacts.digest(app),
            application_slot_bytes=0x200000, slot_free_bytes=0x200000-len(app),
            application_budget_remaining_bytes=0x180000-len(app), growth_target_passed=True))
        files = {'manifest.json': artifacts.json_bytes(manifest), 'firmware.factory.bin': factory,
                 'athan-audio.bin': audio, 'build-report.json': artifacts.json_bytes(report)}
        files = {artifacts.asset_name(hardware, name): data for name, data in files.items()}
        files[artifacts.asset_name(hardware, 'SHA256SUMS')] = artifacts.checksums(files)
        return files, app

    def test_combined_bundle_and_source_binding(self):
        atom, _ = self.bundle(artifacts.HARDWARE)
        waveshare, _ = self.bundle(artifacts.WAVESHARE)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name, data in {**atom, **waveshare}.items(): (directory/name).write_bytes(data)
            manifest, _, files, _ = artifacts.validate_bundle(directory)
            self.assertEqual(manifest['hardware'], artifacts.HARDWARE)
            self.assertEqual(len(files), 9)  # Five assets per board, shared audio once.
            changed, _ = self.bundle(artifacts.WAVESHARE, 'b'*40)
            for name, data in changed.items(): (directory/name).write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'different source'):
                artifacts.validate_bundle(directory)

    def test_waveshare_signature_cannot_be_used_for_atom(self):
        _, app = self.bundle(artifacts.WAVESHARE)
        key = ec.generate_private_key(ec.SECP256R1())
        private = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        public = key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        envelope = upgrade.sign('v0.5.0', 'a'*40, app, private, public, artifacts.WAVESHARE)
        self.assertEqual(upgrade.validate(envelope, app, public, 'v0.5.0', 'a'*40, artifacts.WAVESHARE)['hardware'], artifacts.WAVESHARE)
        with self.assertRaises(ValueError): upgrade.validate(envelope, app, public, 'v0.5.0', 'a'*40)

    def test_production_rejects_isolated_material(self):
        factory, _ = firmware(0x1000000)
        app = esp_image(b'oa_test\0', flash_bytes=0x1000000)
        factory = factory[:0x10000] + app
        with self.assertRaisesRegex(ValueError, 'Isolated'):
            artifacts.validate_firmware_images(factory, app, hardware=artifacts.WAVESHARE, flash_bytes=0x1000000)

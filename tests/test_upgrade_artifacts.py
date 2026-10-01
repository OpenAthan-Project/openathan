"""Production signature verifier and release compatibility contract."""
import json
from pathlib import Path
import struct
import sys
import unittest
import tempfile
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import upgrade_artifacts as upgrade
from release_fixtures import esp_image
from release_fixtures import firmware
import release_artifacts as artifacts
import audio_image

class UpgradeArtifactsTests(unittest.TestCase):
    def setUp(self):
        key = ec.generate_private_key(ec.SECP256R1())
        self.private = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                         serialization.NoEncryption())
        self.public = key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        payload = bytearray(256)
        struct.pack_into("<I", payload, 0, 0xabcd5432)
        payload[16:22] = b"v0.2.0"
        self.app = esp_image(bytes(payload))
        self.commit = "a" * 40
        self.envelope = upgrade.sign("v0.2.0", self.commit, self.app, self.private, self.public)

    def test_verified_application(self):
        result = upgrade.validate(self.envelope, self.app, self.public, "v0.2.0", self.commit)
        self.assertEqual(result["bytes"], len(self.app))
        self.assertTrue(result["rollback"])

    def test_payload_tampering(self):
        changed = json.loads(self.envelope)
        changed["payload"] = changed["payload"].replace("v0.2.0", "v0.3.0")
        with self.assertRaises(ValueError):
            upgrade.validate(json.dumps(changed).encode(), self.app, self.public, "v0.2.0", self.commit)

    def test_wrong_key(self):
        key = ec.generate_private_key(ec.SECP256R1()).public_key()
        pem = key.public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        with self.assertRaises(ValueError):
            upgrade.validate(self.envelope, self.app, pem, "v0.2.0", self.commit)
        with self.assertRaises(ValueError):
            upgrade.sign("v0.2.0", self.commit, self.app, self.private, pem)

    def test_identity_and_image_mismatch(self):
        for version, commit, app in [("v0.3.0", self.commit, self.app), ("v0.2.0", "b"*40, self.app),
                                      ("v0.2.0", self.commit, self.app[:-1]+b"x")]:
            with self.assertRaises(ValueError):
                upgrade.validate(self.envelope, app, self.public, version, commit)

    def test_duplicate_envelope_key_and_bounds(self):
        for data in [b'{"payload":"a","payload":"b","signature":"00"}', b"x"*8193]:
            with self.assertRaises(ValueError):
                upgrade.validate(data, self.app, self.public, "v0.2.0", self.commit)

    def test_signed_bundle_preserves_installer_contract(self):
        factory, _ = firmware()
        factory = factory[:0x10000] + self.app
        audio, _ = audio_image.build_image(b'ID3-synthetic-normal', b'ID3-synthetic-fajr')
        files = {'firmware.factory.bin': factory, 'athan-audio.bin': audio,
                 'firmware.ota.bin': self.app, 'upgrade.json': self.envelope,
                 'manifest.json': artifacts.json_bytes(artifacts.make_manifest(self.commit, 'v0.2.0', factory, audio)),
                 'build-report.json': artifacts.json_bytes(dict(schema=1,commit=self.commit,configuration=artifacts.CONFIGURATION,
                     capacity=dict(application_bytes=len(self.app),application_sha256=artifacts.digest(self.app),
                         application_slot_bytes=0x200000,slot_free_bytes=0x200000-len(self.app),
                         application_budget_remaining_bytes=0x180000-len(self.app),growth_target_passed=True)))}
        files['SHA256SUMS'] = artifacts.checksums(files)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, data in files.items():
                (root / name).write_bytes(data)
            manifest, _, checked, _ = artifacts.validate_bundle(root)
            self.assertEqual(manifest['schema'], 1)
            self.assertEqual(len(manifest['parts']), 2)
            self.assertEqual(set(checked), set(files))
            (root / 'firmware.ota.bin').unlink()
            with self.assertRaises(ValueError):
                artifacts.validate_bundle(root)

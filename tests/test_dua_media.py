"""Synthetic host-only media: never install or play these fixtures."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import dua_media as dua
import audio_image
import release_artifacts

class DuaMediaTests(unittest.TestCase):
    def setUp(self):
        private = ec.generate_private_key(ec.SECP256R1())
        self.private = private.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        self.public = private.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        self.audio = b'HOST ONLY - NOT PLAYABLE' * 101
        self.envelope = dua.sign('v0.8.0', self.audio, self.private, self.public)
        self.bank = dua.bank(self.audio, self.envelope)

    def test_signed_roundtrip_and_legacy_reader(self):
        self.assertEqual(dua.read_bank(self.bank, self.public), (self.audio, self.envelope, 1))
        old, _ = audio_image.build_image(b'normal fixture', b'fajr fixture')
        new, _ = audio_image.build_image(b'normal fixture', b'fajr fixture', self.audio, self.envelope)
        self.assertEqual(new[:dua.BANK_START], old[:dua.BANK_START])
        self.assertEqual(release_artifacts.audio_payloads(new, self.public), [b'normal fixture', b'fajr fixture', self.audio])
        validator = os.environ.get('AUDIO_VALIDATOR', str(ROOT / 'build/audio_validator'))
        with tempfile.NamedTemporaryFile() as f:
            f.write(new); f.flush()
            subprocess.run([validator, f.name], check=True)

    def test_corrupt_metadata_signature_payload_and_uncommitted(self):
        for offset in (0, 8, 12, 16, 20, 24, 64, 4060, 4092, 4096):
            with self.subTest(offset=offset):
                damaged = bytearray(self.bank); damaged[offset] ^= 1
                with self.assertRaises(Exception): dua.read_bank(damaged, self.public)
        other = ec.generate_private_key(ec.SECP256R1()).public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        with self.assertRaises(Exception): dua.validate(self.envelope, self.audio, other)
        with self.assertRaises(ValueError): dua.validate(self.envelope, self.audio, self.public, 'v0.8.1')

    def test_torn_payload_header_and_commit_preserve_other_bank(self):
        # Model NOR writes (1 -> 0 only), including a cut inside every written chunk.
        old = dua.bank(self.audio, self.envelope, 1)
        new = dua.bank(self.audio, self.envelope, 2)
        for cut in (0, 1, 4095, 4096, 4096 + len(self.audio) - 1, dua.BANK_SIZE):
            staging = bytearray(b'\xff' * dua.BANK_SIZE)
            staging[4096:cut] = new[4096:cut]
            self.assertEqual(dua.read_bank(old, self.public)[2], 1)
            with self.assertRaises(ValueError): dua.read_bank(staging, self.public)
        for cut in (0, 1, 63, 2048, 4060, 4092, 4093, 4094, 4095):
            staging = bytearray(b'\xff' * dua.BANK_SIZE); staging[4096:] = new[4096:]; staging[:cut] = new[:cut]
            with self.assertRaises(ValueError): dua.read_bank(staging, self.public)
        self.assertEqual(dua.read_bank(new, self.public)[2], 2)

    def test_limits_overlap_and_canonical_padding(self):
        with self.assertRaises(ValueError): dua.bank(b'x' * (dua.MAX_BYTES + 1), self.envelope)
        with self.assertRaises(ValueError): dua.bank(self.audio, b'x' * 2049)
        with self.assertRaises(ValueError): audio_image.build_image(b'x' * dua.BANK_START, b'y', self.audio, self.envelope)
        with self.assertRaises(ValueError): audio_image.build_image(b'x', b'y', self.audio)
        for offset in (63, 3000, 4096 + len(self.audio)):
            bad = bytearray(self.bank); bad[offset] = 0
            bad[4060:4092] = hashlib.sha256(bad[:4060]).digest()
            with self.assertRaises(ValueError): dua.read_bank(bad, self.public)

    def test_release_bundle_binds_downloads_to_fresh_install_media(self):
        import release
        from release_fixtures import firmware
        artifacts = release_artifacts
        normal, fajr = b'host-only normal', b'host-only fajr'
        audio, _ = audio_image.build_image(normal, fajr, self.audio, self.envelope)
        files = {'dua.json': self.envelope, 'dua.mp3': self.audio, 'athan-audio.bin': audio}
        commit = 'a' * 40
        for hardware, profile in artifacts.PROFILES.items():
            factory, app = firmware(profile['flash_bytes'])
            files.update({artifacts.asset_name(hardware, 'firmware.factory.bin'): factory,
                artifacts.asset_name(hardware, 'manifest.json'): artifacts.json_bytes(artifacts.make_manifest(commit, 'v0.8.0', factory, audio, hardware)),
                artifacts.asset_name(hardware, 'build-report.json'): artifacts.json_bytes(dict(schema=1,commit=commit,configuration=profile['configuration'],
                    capacity=dict(application_bytes=len(app),application_sha256=artifacts.digest(app),application_slot_bytes=0x200000,
                        slot_free_bytes=0x200000-len(app),application_budget_remaining_bytes=0x180000-len(app),growth_target_passed=True)))})
        for hardware, profile in artifacts.PROFILES.items():
            chosen={k:v for k,v in files.items() if k in ('dua.json','dua.mp3','athan-audio.bin') or
                (k.startswith(profile['prefix']) if hardware==artifacts.WAVESHARE else not k.startswith(artifacts.profile(artifacts.WAVESHARE)['prefix']))}
            files[artifacts.asset_name(hardware, 'SHA256SUMS')]=artifacts.checksums(chosen)
        with tempfile.TemporaryDirectory() as temporary, patch.object(release, 'source_file', return_value=self.public):
            root=Path(temporary)
            for name,data in files.items(): (root/name).write_bytes(data)
            manifest,_,checked,payloads=artifacts.validate_bundle(root)
            self.assertEqual(payloads,[normal,fajr,self.audio]);self.assertEqual(len(manifest['parts']),2)
            (root/'dua.mp3').write_bytes(b'changed')
            with self.assertRaises(ValueError): artifacts.validate_bundle(root)

    def test_duplicate_keys_are_rejected(self):
        with self.assertRaises(ValueError): dua.validate(b'{"payload":"a","payload":"b","signature":"00"}', self.audio, self.public)

    def test_large_legacy_audio_is_not_mistaken_for_optional_banks(self):
        normal, fajr = b'n' * (dua.BANK_START + 16), b'f' * 4096
        media, _ = audio_image.build_image(normal, fajr)
        self.assertEqual(release_artifacts.audio_payloads(media), [normal, fajr])

    def test_packaging_checks_decodable_mp3_and_duration(self):
        import release
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            for seconds in (1,31):
                path=root/(str(seconds)+'.mp3')
                subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','anullsrc=r=22050:cl=mono','-t',str(seconds),
                                '-c:a','libmp3lame','-b:a','32k',str(path)],check=True)
                if seconds==1: release.validate_dua_recording(path)
                else:
                    with self.assertRaises(ValueError): release.validate_dua_recording(path)
            bad=root/'bad.mp3';bad.write_bytes(b'text without audio')
            with self.assertRaises(subprocess.CalledProcessError): release.validate_dua_recording(bad)

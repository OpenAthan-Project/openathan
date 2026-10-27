"""Board-bound release assets, using non-executable fixtures only."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import release_artifacts as artifacts
import release
import upgrade_artifacts as upgrade
from audio_image import build_image
from release_fixtures import firmware, esp_image, registry


class HardwareReleaseTests(unittest.TestCase):
    def bundle(self, hardware, commit='a'*40, phone_setup=None, *, embedded_hardware=True):
        profile = artifacts.profile(hardware)
        factory, _ = firmware(profile['flash_bytes'])
        payload = bytearray(256)
        struct.pack_into('<I', payload, 0, 0xabcd5432)
        payload[16:22] = b'v0.5.0'
        if embedded_hardware:
            payload += hardware.encode() + b'\0'
        app = esp_image(payload, flash_bytes=profile['flash_bytes'])
        factory = factory[:0x10000] + app
        audio, _ = build_image(b'ID3-test-normal', b'ID3-test-fajr')
        manifest = artifacts.make_manifest(commit, 'v0.5.0', factory, audio, hardware,
                                           phone_setup=phone_setup)
        report = dict(schema=1, commit=commit, configuration=profile['configuration'], capacity=dict(
            application_bytes=len(app), application_sha256=artifacts.digest(app),
            application_slot_bytes=0x200000, slot_free_bytes=0x200000-len(app),
            application_budget_remaining_bytes=0x180000-len(app), growth_target_passed=True))
        files = {'manifest.json': artifacts.json_bytes(manifest), 'firmware.factory.bin': factory,
                 'athan-audio.bin': audio, 'build-report.json': artifacts.json_bytes(report)}
        files = {artifacts.asset_name(hardware, name): data for name, data in files.items()}
        files[artifacts.asset_name(hardware, 'SHA256SUMS')] = artifacts.checksums(files)
        return files, app

    def test_optional_phone_setup_capability_on_both_profiles(self):
        for hardware in artifacts.PROFILES:
            for capability in (None, 1):
                with self.subTest(hardware=hardware, capability=capability):
                    files, _ = self.bundle(hardware, phone_setup=capability)
                    with tempfile.TemporaryDirectory() as temporary:
                        directory = Path(temporary)
                        for name, data in files.items():
                            (directory / name).write_bytes(data)
                        manifest, _, _, _ = artifacts.validate_bundle(directory)
                    if capability is None:
                        self.assertNotIn('phoneSetup', manifest)
                    else:
                        self.assertEqual(manifest['phoneSetup'], 1)

    def test_malformed_phone_setup_capability_is_rejected(self):
        for hardware in artifacts.PROFILES:
            original, _ = self.bundle(hardware)
            name = artifacts.asset_name(hardware, 'manifest.json')
            checksum = artifacts.asset_name(hardware, 'SHA256SUMS')
            for value in (True, False, 1.0, 0, 2, '1', None, [], {}):
                with self.subTest(hardware=hardware, value=value):
                    files = dict(original)
                    manifest = artifacts.read_json(files[name])
                    manifest['phoneSetup'] = value
                    files[name] = artifacts.json_bytes(manifest)
                    files.pop(checksum)
                    files[checksum] = artifacts.checksums(files)
                    with tempfile.TemporaryDirectory() as temporary:
                        directory = Path(temporary)
                        for file, data in files.items():
                            (directory / file).write_bytes(data)
                        with self.assertRaisesRegex(ValueError, 'phoneSetup'):
                            artifacts.validate_bundle(directory)
                    if value is not None:
                        with self.assertRaisesRegex(ValueError, 'phoneSetup'):
                            artifacts.make_manifest('a'*40, 'v0.5.0', files[artifacts.asset_name(hardware, 'firmware.factory.bin')],
                                                    files['athan-audio.bin'], hardware, phone_setup=value)

    def package_source(self, directory, hardware, source_config):
        """Exercise the packager with committed metadata and synthetic build bytes."""
        files, app = self.bundle(hardware)
        factory = files[artifacts.asset_name(hardware, 'firmware.factory.bin')]
        report = artifacts.read_json(files[artifacts.asset_name(hardware, 'build-report.json')])
        report.update(source_tree='b'*40, toolchain={})
        payloads = (b'ID3-test-normal', b'ID3-test-fajr')
        approved = registry(payloads)
        source = {
            artifacts.MEDIA_REGISTRY: artifacts.json_bytes(approved),
            artifacts.LICENSE_PATH: '\n'.join(track['sha256'] for track in approved['tracks']).encode(),
        }
        build = directory / 'build'
        build.mkdir()
        (build / 'firmware.factory.bin').write_bytes(factory)
        (build / 'firmware.ota.bin').write_bytes(app)
        for name, payload in zip(('normal.mp3', 'fajr.mp3'), payloads):
            (directory / name).write_bytes(payload)
        key_path = None
        if source_config is not None:
            key = ec.generate_private_key(ec.SECP256R1())
            key_path = directory / 'test-only-key.pem'
            key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                                  serialization.NoEncryption()))
            source.update({
                'release/firmware.json': artifacts.json_bytes(source_config),
                'release/upgrade-public-key.pem': key.public_key().public_bytes(
                    serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo),
                'release/rollback-bootloaders.json': artifacts.json_bytes(
                    dict(regionBytes=32768, sha256=[artifacts.digest(factory[:32768])])),
            })
        def source_file(commit, name):
            self.assertEqual(commit, 'a'*40)
            return source[name]
        with patch.object(release, 'validate_build', return_value=report), \
                patch.object(release, 'source_file', side_effect=source_file), \
                patch.object(release, 'git', return_value=b'release/firmware.json' if source_config is not None else b''), \
                patch.object(release, 'validate_audio_cpp'):
            release.package(build, 'v0.5.0', directory / 'normal.mp3', directory / 'fajr.mp3',
                            directory / 'bundle', key_path)
        return directory / 'bundle'

    def test_package_uses_only_explicit_committed_phone_setup_opt_in(self):
        for hardware in artifacts.PROFILES:
            for config in (None, {'version': 'v0.5.0'}, {'version': 'v0.5.0', 'phoneSetup': 1}):
                with self.subTest(hardware=hardware, config=config), tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary)
                    bundle = self.package_source(directory, hardware, config)
                    manifest, _, _, _ = artifacts.validate_bundle(bundle)
                    if config is not None and 'phoneSetup' in config:
                        self.assertEqual(manifest['phoneSetup'], 1)
                    else:
                        self.assertNotIn('phoneSetup', manifest)

    def test_package_rejects_invalid_source_capability_before_output(self):
        for value in (True, 1.0, 0, 2, '1', None):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                with self.assertRaisesRegex(ValueError, 'phoneSetup'):
                    self.package_source(directory, artifacts.HARDWARE, {'version': 'v0.5.0', 'phoneSetup': value})
                self.assertFalse((directory / 'bundle').exists())

    def test_upload_rejects_capability_different_from_committed_source(self):
        for hardware in artifacts.PROFILES:
            for source_capability in (None, 1):
                with self.subTest(hardware=hardware, capability=source_capability), tempfile.TemporaryDirectory() as temporary:
                    config = {'version': 'v0.5.0'}
                    if source_capability is not None:
                        config['phoneSetup'] = source_capability
                    bundle = self.package_source(Path(temporary), hardware, config)
                    name = artifacts.asset_name(hardware, 'manifest.json')
                    checksum = artifacts.asset_name(hardware, 'SHA256SUMS')
                    files = {path.name: path.read_bytes() for path in bundle.iterdir() if path.name != checksum}
                    manifest = artifacts.read_json(files[name])
                    if source_capability is None:
                        manifest['phoneSetup'] = 1
                    else:
                        del manifest['phoneSetup']
                    files[name] = artifacts.json_bytes(manifest)
                    files[checksum] = artifacts.checksums(files)
                    for file, data in files.items():
                        (bundle / file).write_bytes(data)
                    artifacts.validate_bundle(bundle)
                    with patch.object(release, 'source_commit'), \
                            patch.object(release, 'git', return_value=b'release/firmware.json'), \
                            patch.object(release, 'source_file', return_value=artifacts.json_bytes(config)), \
                            patch.object(release, 'GitHub') as github:
                        with self.assertRaisesRegex(ValueError, 'differs from committed'):
                            release.upload_draft(bundle)
                        github.assert_not_called()

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

    def test_three_board_bundle_and_incomplete_assets(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for hardware in artifacts.PROFILES:
                files, _ = self.bundle(hardware)
                for name, data in files.items(): (directory / name).write_bytes(data)
            _, _, files, _ = artifacts.validate_bundle(directory)
            self.assertEqual(len(files), 13)
            (directory / artifacts.asset_name(artifacts.AUDIO_BOARD, 'firmware.factory.bin')).unlink()
            with self.assertRaises(ValueError): artifacts.validate_bundle(directory)

    def test_audio_board_signature_is_not_box_v2_despite_same_flash_size(self):
        _, app = self.bundle(artifacts.AUDIO_BOARD)
        key = ec.generate_private_key(ec.SECP256R1())
        private = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
        public = key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        envelope = upgrade.sign('v0.5.0', 'a'*40, app, private, public, artifacts.AUDIO_BOARD)
        self.assertEqual(upgrade.validate(envelope, app, public, 'v0.5.0', 'a'*40, artifacts.AUDIO_BOARD)['hardware'], artifacts.AUDIO_BOARD)
        with self.assertRaises(ValueError): upgrade.validate(envelope, app, public, 'v0.5.0', 'a'*40, artifacts.WAVESHARE)

    def test_same_capacity_images_cannot_be_relabelled_or_signed(self):
        key = ec.generate_private_key(ec.SECP256R1())
        private = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                    serialization.NoEncryption())
        public = key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        for source, target in ((artifacts.WAVESHARE, artifacts.AUDIO_BOARD),
                               (artifacts.AUDIO_BOARD, artifacts.WAVESHARE)):
            with self.subTest(source=source, target=target):
                files, app = self.bundle(source)
                factory = files[artifacts.asset_name(source, 'firmware.factory.bin')]
                with self.assertRaisesRegex(ValueError, 'hardware identity'):
                    artifacts.validate_firmware_images(factory, app, flash_bytes=0x1000000, hardware=target)
                with self.assertRaisesRegex(ValueError, 'hardware identity'):
                    upgrade.sign('v0.5.0', 'a'*40, app, private, public, target)

    def test_legacy_markerless_bundles_remain_supported(self):
        for hardware in (artifacts.HARDWARE, artifacts.WAVESHARE):
            with self.subTest(hardware=hardware):
                files, app = self.bundle(hardware, embedded_hardware=False)
                artifacts.validate_profile_files(files, hardware)
                self.assertEqual(upgrade.metadata('v0.5.0', 'a'*40, app, hardware)['hardware'], hardware)

    def test_audio_board_requires_embedded_identity(self):
        files, app = self.bundle(artifacts.AUDIO_BOARD, embedded_hardware=False)
        with self.assertRaisesRegex(ValueError, 'missing its embedded hardware identity'):
            artifacts.validate_profile_files(files, artifacts.AUDIO_BOARD)
        with self.assertRaisesRegex(ValueError, 'missing its embedded hardware identity'):
            upgrade.metadata('v0.5.0', 'a'*40, app, artifacts.AUDIO_BOARD)

    def test_ambiguous_embedded_hardware_is_rejected(self):
        files, original = self.bundle(artifacts.WAVESHARE)
        app = esp_image(original[32:288] + artifacts.WAVESHARE.encode() + b'\0' +
                        artifacts.AUDIO_BOARD.encode() + b'\0', flash_bytes=0x1000000)
        factory = files[artifacts.asset_name(artifacts.WAVESHARE, 'firmware.factory.bin')][:0x10000] + app
        for hardware in (artifacts.WAVESHARE, artifacts.AUDIO_BOARD):
            with self.subTest(hardware=hardware), self.assertRaisesRegex(ValueError, 'hardware identity'):
                artifacts.validate_firmware_images(factory, app, hardware=hardware, flash_bytes=0x1000000)

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

"""Release checks use synthetic bytes, temporary repositories and a fake GitHub."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import audio_image
import release
import release_artifacts as artifacts
from release_fixtures import compiled, esp_image, firmware, registry


class FakeGitHub:
    def __init__(self, commit):
        self.commit = commit
        self.main = commit
        self.success = True
        self.releases = []
        self.assets = {}
        self.calls = []
        self.fail_upload = None
        self.corrupt_download = False
        self.extra_runs = []

    def tag_commit(self, tag):
        return self.commit

    def api(self, path, paginate=False):
        if path.startswith('compare/'):
            return {'merge_base_commit': {'sha': self.main}}
        if path.startswith('actions/'):
            return {'workflow_runs': [dict(id=1, head_sha=self.commit, head_branch='main', event='push',
                                           head_repository={'full_name': artifacts.REPOSITORY}, status='completed',
                                           conclusion='success' if self.success else 'failure'), *self.extra_runs]}
        if path.startswith('releases?'):
            return copy.deepcopy(self.releases)
        if path.endswith('/assets?per_page=100'):
            return [dict(id=i, name=name, state='uploaded', size=len(data))
                    for i, (name, data) in enumerate(self.assets.items())]
        raise AssertionError(path)

    def asset_bytes(self, asset):
        return b'corrupt' if self.corrupt_download else self.assets[asset['name']]

    def call(self, *args):
        self.calls.append(args)
        if args[:2] == ('release', 'create'):
            self.releases.append(dict(id=7, tag_name=args[2], draft=True, prerelease=False, published_at=None,
                                      html_url='https://github.com/OpenAthan-Project/openathan/releases/tag/' + args[2]))
        if args[:2] == ('release', 'upload'):
            path = Path(args[3])
            if path.name == self.fail_upload:
                raise subprocess.CalledProcessError(1, ['gh'], stderr=b'network interruption')
            self.assets[path.name] = path.read_bytes()
        return b''


class ReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.validator = Path(os.environ.get('AUDIO_VALIDATOR', ROOT / 'build/audio_validator'))
        if not cls.validator.is_file():
            raise AssertionError('Build audio_validator before running release tests')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.repo = self.base / 'repo'
        self.repo.mkdir()
        self.payloads = (b'ID3-synthetic-normal', b'ID3-synthetic-fajr')
        self.registry = registry(self.payloads)
        files = {
            artifacts.MEDIA_REGISTRY: artifacts.json_bytes(self.registry),
            artifacts.LICENSE_PATH: ('Test-only media hashes\n' + '\n'.join(t['sha256'] for t in self.registry['tracks'])).encode(),
            artifacts.PINS_PATH: (ROOT / artifacts.PINS_PATH).read_bytes(),
            'firmware/esphome/feasibility/partitions.csv': audio_image.PARTITIONS.read_bytes(),
            artifacts.CONFIGURATION: b'# test-only source\n',
        }
        for name, data in files.items():
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        def git(*args):
            return subprocess.check_output(['git', '-C', str(self.repo), *args], stderr=subprocess.DEVNULL)
        git('init', '-q')
        git('config', 'user.name', 'Release test')
        git('config', 'user.email', 'test@example.org')
        git('add', '.')
        git('commit', '-qm', 'Synthetic release test')
        self.commit = git('rev-parse', 'HEAD').decode().strip()
        self.tree = git('rev-parse', 'HEAD^{tree}').decode().strip()
        self.addCleanup(patch.stopall)
        patch.object(release, 'ROOT', self.repo).start()
        self.pins = json.loads(files[artifacts.PINS_PATH])
        self.build = self.base / 'build-output'
        self.build.mkdir()
        release.export_source(self.commit, self.build / 'source')
        (self.build / 'source.tar').write_bytes(git('archive', '--format=tar', self.commit))
        compiled(self.build / 'compiled/openathan', self.pins, audio_image.PARTITIONS.read_bytes())
        factory, app = firmware()
        (self.build / 'firmware.factory.bin').write_bytes(factory)
        (self.build / 'firmware.ota.bin').write_bytes(app)
        (self.build / 'compile.log').write_text('RAM: used 100 bytes from 341760 bytes\n'
                                               'Flash: used 200 bytes from 2097152 bytes\nSuccessfully compiled program\n')
        capacity = release.check_feasibility.inspect_firmware(self.build / 'compiled/openathan', self.build / 'compile.log')
        record = dict(schema=1, commit=self.commit, source_tree=self.tree, configuration=artifacts.CONFIGURATION,
                      capacity=capacity, toolchain=dict(python='3.13.0', esphome=self.pins['esphome'],
                          esp_idf=self.pins['esp_idf'], compiler=self.pins['xtensa_esp_elf'], tzdata='2026.4'))
        record['evidence'] = {name: artifacts.digest((self.build / name).read_bytes()) for name in
                             ('source.tar', 'compile.log', 'firmware.factory.bin', 'firmware.ota.bin')}
        (self.build / 'build-record.json').write_bytes(artifacts.json_bytes(record))
        for name, data in zip(('normal.mp3', 'fajr.mp3'), self.payloads):
            (self.base / name).write_bytes(data)
        self.bundle = self.base / 'bundle'
        # Run the actual production validator, already compiled by the host suite.
        patch.object(release, 'validate_audio_cpp', side_effect=lambda commit, path:
                     subprocess.run([self.validator, path], check=True, capture_output=True)).start()
        self.github = FakeGitHub(self.commit)

    def package(self):
        return release.package(self.build, 'v0.1.0', self.base / 'normal.mp3', self.base / 'fajr.mp3', self.bundle)

    def upload(self):
        return release.upload_draft(self.bundle, self.github)

    def rewrite_checksums(self):
        files = {p.name: p.read_bytes() for p in self.bundle.iterdir()}
        (self.bundle / 'SHA256SUMS').write_bytes(artifacts.checksums(files))

    def test_build_uses_exported_source_and_retains_validated_evidence(self):
        original_run = subprocess.run
        (self.repo / 'secrets.yaml').write_text('private sentinel')
        def run(args, **kwargs):
            if list(args[:3]) == [sys.executable, '-m', 'esphome']:
                self.assertEqual(args[-1], artifacts.CONFIGURATION)
                self.assertFalse((Path(kwargs['cwd']) / 'secrets.yaml').exists())
                compiled(Path(kwargs['env']['ESPHOME_BUILD_PATH']) / 'openathan',
                         self.pins, audio_image.PARTITIONS.read_bytes())
                kwargs['stdout'].write((self.build / 'compile.log').read_bytes())
                return subprocess.CompletedProcess(args, 0)
            return original_run(args, **kwargs)
        output = self.base / 'fresh-build'
        with patch.object(release.subprocess, 'run', side_effect=run):
            record = release.build(self.commit, output)
        self.assertEqual(record, release.validate_build(output))
        self.assertFalse((output / 'source/secrets.yaml').exists())

    def test_failed_compile_never_exposes_finished_output(self):
        original_run = subprocess.run
        def run(args, **kwargs):
            if list(args[:3]) == [sys.executable, '-m', 'esphome']:
                kwargs['stdout'].write(b'injected compilation failure')
                return subprocess.CompletedProcess(args, 1)
            return original_run(args, **kwargs)
        output = self.base / 'failed-build'
        with patch.object(release.subprocess, 'run', side_effect=run):
            with self.assertRaisesRegex(ValueError, 'compilation failed'):
                release.build(self.commit, output)
        self.assertFalse(output.exists())

    def test_package_matches_contract_and_preserves_payloads(self):
        result = self.package()
        manifest, report, files, payloads = artifacts.validate_bundle(self.bundle)
        self.assertEqual(tuple(payloads), self.payloads)
        self.assertEqual(manifest['commit'], self.commit)
        self.assertEqual(manifest['parts'][1]['offset'], 0x410000)
        self.assertEqual(result['manifest_sha256'], artifacts.digest(files['manifest.json']))
        self.assertNotIn(str(self.base), json.dumps(report))
        self.assertEqual(set(files), set(artifacts.ASSETS))

    def test_unapproved_registry_is_rejected(self):
        unapproved = copy.deepcopy(self.registry)
        unapproved['tracks'][0]['approved'] = False
        with self.assertRaisesRegex(ValueError, 'not been approved'):
            artifacts.approved_tracks(unapproved, release.source_file(self.commit, artifacts.LICENSE_PATH), self.payloads)

    def test_media_approval_is_from_commit_not_working_tree(self):
        (self.repo / artifacts.MEDIA_REGISTRY).write_text('{"schema":1,"tracks":[]}')
        self.package()  # Working-tree edits do not replace the reviewed commit.
        (self.base / 'fajr.mp3').write_bytes(b'different recording')
        with self.assertRaisesRegex(ValueError, 'approved hash'):
            release.package(self.build, 'v0.1.0', self.base / 'normal.mp3', self.base / 'fajr.mp3', self.base / 'other')
        self.assertFalse((self.base / 'other').exists())

    def test_missing_or_malformed_approval_metadata(self):
        for key, value in [('approved', False), ('license', None), ('source', 'file:///private/path'), ('attribution', '')]:
            registry_bad = copy.deepcopy(self.registry)
            registry_bad['tracks'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                artifacts.approved_tracks(registry_bad, release.source_file(self.commit, artifacts.LICENSE_PATH), self.payloads)
        with self.assertRaisesRegex(ValueError, 'identify'):
            artifacts.approved_tracks(self.registry, b'missing hashes', self.payloads)

    def test_changed_build_evidence_and_incomplete_output_are_rejected(self):
        (self.build / 'firmware.factory.bin').write_bytes(b'wrong')
        with self.assertRaisesRegex(ValueError, 'evidence changed'):
            self.package()
        self.assertFalse(self.bundle.exists())

    def test_outputs_are_fresh_outside_repo_and_atomic(self):
        for path in (self.build, self.repo / 'output'):
            with self.assertRaises(ValueError):
                with release.fresh_output(path):
                    self.fail('must not enter')
        link = self.base / 'link'
        link.symlink_to(self.bundle)
        with self.assertRaises(ValueError):
            with release.fresh_output(link):
                self.fail('must not enter')
        with self.assertRaisesRegex(RuntimeError, 'injected'):
            with release.fresh_output(self.bundle) as stage:
                (stage / 'partial').write_text('partial')
                raise RuntimeError('injected')
        self.assertFalse(self.bundle.exists())

    def test_export_excludes_untracked_settings_and_rejects_branch_names(self):
        (self.repo / 'secrets.yaml').write_text('private sentinel')
        path = self.base / 'export'
        release.export_source(self.commit, path)
        self.assertFalse((path / 'secrets.yaml').exists())
        with self.assertRaisesRegex(ValueError, 'full lowercase'):
            release.source_commit('main')

    def test_bundle_rejects_missing_extra_symlink_and_changed_assets(self):
        self.package()
        original = (self.bundle / 'athan-audio.bin').read_bytes()
        (self.bundle / 'athan-audio.bin').unlink()
        with self.assertRaises(ValueError):
            artifacts.validate_bundle(self.bundle)
        (self.bundle / 'athan-audio.bin').symlink_to(self.base / 'fajr.mp3')
        with self.assertRaises(ValueError):
            artifacts.validate_bundle(self.bundle)
        (self.bundle / 'athan-audio.bin').unlink()
        (self.bundle / 'athan-audio.bin').write_bytes(original)
        (self.bundle / 'private.txt').write_text('must not upload')
        with self.assertRaises(ValueError):
            self.upload()
        self.assertFalse(self.github.calls)

    def test_manifest_changes_rejected_even_with_new_checksums(self):
        self.package()
        original = (self.bundle / 'manifest.json').read_bytes()
        for field, value in [('chip', 'ESP32'), ('layout', 'wrong'), ('hardware', 'wrong'), ('tag', 'v0.1.0-rc1'), ('schema', True)]:
            manifest = artifacts.read_json(original)
            manifest[field] = value
            (self.bundle / 'manifest.json').write_bytes(artifacts.json_bytes(manifest))
            self.rewrite_checksums()
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.upload()
        self.assertFalse(self.github.calls)

    def test_corrupt_audio_is_rejected_after_hashes_are_updated(self):
        self.package()
        files = {p.name: p.read_bytes() for p in self.bundle.iterdir()}
        media = bytearray(files['athan-audio.bin'])
        media[4096] ^= 1
        (self.bundle / 'athan-audio.bin').write_bytes(media)
        manifest = artifacts.make_manifest(self.commit, 'v0.1.0', files['firmware.factory.bin'], media)
        (self.bundle / 'manifest.json').write_bytes(artifacts.json_bytes(manifest))
        self.rewrite_checksums()
        with self.assertRaisesRegex(ValueError, 'track hash'):
            self.upload()
        self.assertFalse(self.github.calls)

    def test_draft_upload_and_identical_retry(self):
        self.package()
        result = self.upload()
        self.assertEqual(result['status'], 'draft')
        self.assertEqual(set(self.github.assets), set(artifacts.ASSETS))
        created = next(call for call in self.github.calls if call[:2] == ('release', 'create'))
        self.assertIn('--draft', created)
        self.assertIn('--verify-tag', created)
        self.assertFalse(any('--clobber' in call for call in self.github.calls))
        count = len([c for c in self.github.calls if c[:2] == ('release', 'upload')])
        self.upload()
        self.assertEqual(count, len([c for c in self.github.calls if c[:2] == ('release', 'upload')]))

    def test_partial_upload_can_resume_missing_assets(self):
        self.package()
        self.github.fail_upload = 'athan-audio.bin'
        with self.assertRaises(subprocess.CalledProcessError):
            self.upload()
        self.assertEqual(set(self.github.assets), {'manifest.json', 'firmware.factory.bin'})
        self.github.fail_upload = None
        self.upload()
        self.assertEqual(set(self.github.assets), set(artifacts.ASSETS))
        self.assertEqual(sum(c[:2] == ('release', 'create') for c in self.github.calls), 1)

    def test_remote_tag_main_and_ci_gates(self):
        self.package()
        for key, value in [('commit', 'b' * 40), ('main', 'b' * 40), ('success', False)]:
            github = FakeGitHub(self.commit)
            setattr(github, key, value)
            release.validate_audio_cpp.reset_mock()
            with self.subTest(key=key), self.assertRaises(ValueError):
                release.upload_draft(self.bundle, github)
            self.assertFalse(any(c[0] == 'release' for c in github.calls))
            release.validate_audio_cpp.assert_not_called()
        self.github.extra_runs = [dict(id=2, head_sha=self.commit, head_branch='main', event='push',
                                       head_repository={'full_name': artifacts.REPOSITORY}, status='in_progress', conclusion=None)]
        with self.assertRaisesRegex(ValueError, 'latest'):
            self.upload()

    def test_published_releases_conflicts_and_remote_corruption(self):
        self.package()
        self.upload()
        self.github.releases[0]['draft'] = False
        with self.assertRaisesRegex(ValueError, 'published'):
            self.upload()
        self.github.releases[0]['draft'] = True
        self.github.assets['manifest.json'] = b'changed'
        with self.assertRaisesRegex(ValueError, 'Conflicting'):
            self.upload()
        self.github.assets['manifest.json'] = (self.bundle / 'manifest.json').read_bytes()
        self.github.corrupt_download = True
        with self.assertRaisesRegex(ValueError, 'Conflicting'):
            self.upload()

    def test_public_report_cannot_include_private_fields(self):
        self.package()
        report = artifacts.read_json((self.bundle / 'build-report.json').read_bytes())
        report['raw_log'] = '/private/operator/build.log'
        (self.bundle / 'build-report.json').write_bytes(artifacts.json_bytes(report))
        self.rewrite_checksums()
        with self.assertRaisesRegex(ValueError, 'Unexpected public'):
            self.upload()
        self.assertFalse(self.github.calls)


class ImageTests(unittest.TestCase):
    def test_image_headers_partitions_payload_and_digests(self):
        factory, app = firmware()
        self.assertEqual(artifacts.validate_firmware_images(factory, app), app)
        for offset in (12, 0x8004, 0x9000, 0x1000c, 0x10030):
            bad = bytearray(factory)
            bad[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                artifacts.validate_firmware_images(bad, app)
        with self.assertRaisesRegex(ValueError, 'payloads differ'):
            artifacts.validate_firmware_images(factory, app + b'wrong')
        with self.assertRaises(ValueError):
            artifacts.validate_firmware_images(factory[:-10])

    def test_oversized_actual_app_and_duplicate_metadata(self):
        factory, _ = firmware()
        app = esp_image(b'x' * 0x180000)
        with self.assertRaisesRegex(ValueError, 'growth-budget'):
            artifacts.validate_firmware_images(factory[:0x10000] + app)
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            artifacts.read_json(b'{"schema":1,"schema":2}')


class GitHubAdapterTests(unittest.TestCase):
    def test_forces_public_github_host_and_flattens_paginated_results(self):
        with patch.dict(os.environ, {'GH_HOST': 'example.org'}):
            with patch.object(release, 'command', return_value=b'[[{"id":1}],[{"id":2}]]') as call:
                result = release.GitHub().api('releases?per_page=100', paginate=True)
        self.assertEqual(result, [{'id': 1}, {'id': 2}])
        self.assertEqual(call.call_args.kwargs['env']['GH_HOST'], 'github.com')
        self.assertIn('--slurp', call.call_args.args[0])

    def test_annotated_tags_resolve_to_commit(self):
        github = release.GitHub()
        with patch.object(github, 'api', side_effect=[
            {'object': {'type': 'tag', 'sha': 'b' * 40}},
            {'object': {'type': 'commit', 'sha': 'a' * 40}},
        ]):
            self.assertEqual(github.tag_commit('v0.1.0'), 'a' * 40)

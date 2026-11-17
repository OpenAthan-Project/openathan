import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import athan_catalog as catalog


def keypair():
    key = ec.generate_private_key(ec.SECP256R1())
    return (key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()),
            key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))


def registry():
    tracks = []
    for role in ('normal', 'fajr'):
        content = role.encode()
        tracks.append(dict(id=role, role=role, name=role, bytes=len(content), sha256=hashlib.sha256(content).hexdigest(), file=role + '.mp3', attribution='Author', source='https://example.org/source', license='https://example.org/license', approved=True, review='Approved exact recording', permission='https://example.org/permission', source_sha256=hashlib.sha256(content).hexdigest(), transformation='Unchanged'))
    return dict(schema=1, version='v1.0.0', recordings=tracks)


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.reg = registry()
        self.private, self.public = keypair()

    def payload(self):
        return catalog.canonical(catalog.registry_payload(catalog.canonical(self.reg)))

    def test_sign_and_verify_and_reject_tampering_and_wrong_key(self):
        payload = self.payload()
        signed = catalog.sign(payload, self.private, self.public)
        self.assertEqual(catalog.verify_envelope(signed, self.public), json.loads(payload))
        _, other = keypair()
        with self.assertRaises(ValueError): catalog.sign(payload, self.private, other)
        with self.assertRaises(ValueError): catalog.verify_envelope(signed, other)
        env = json.loads(signed); env['payload'] = env['payload'].replace('normal', 'normax')
        with self.assertRaises(ValueError): catalog.verify_envelope(catalog.canonical(env), self.public)

    def test_pair_boundary_and_alignment(self):
        doc = json.loads(self.payload())
        doc['recordings'][0]['bytes'] = 17
        doc['recordings'][1]['bytes'] = catalog.MAX_USED - 4096 - 32
        catalog.validate_payload(doc)
        doc['recordings'][1]['bytes'] += 1
        with self.assertRaises(ValueError): catalog.validate_payload(doc)

    def test_unapproved_and_incomplete_permission_rejected(self):
        self.reg['recordings'][0]['approved'] = False
        with self.assertRaises(ValueError): catalog.registry_payload(catalog.canonical(self.reg))
        self.reg['recordings'][0].pop('permission')
        with self.assertRaises(ValueError): catalog.registry_payload(catalog.canonical(self.reg), True)
        self.reg['recordings'][0].update(permission='https://example.org/permission', source_sha256='a'*64, transformation='transcoded')
        catalog.registry_payload(catalog.canonical(self.reg), True)
        with self.assertRaises(ValueError): catalog.registry_payload(catalog.canonical(self.reg))

    def test_invalid_metadata(self):
        original = json.loads(self.payload())
        for key, value in [('id', '../x'), ('file', '../x.mp3'), ('sha256', 'a'*63), ('bytes', True), ('name', 'x'*81), ('role', 'dua')]:
            doc = copy.deepcopy(original); doc['recordings'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): catalog.validate_payload(doc)
        with self.assertRaises(ValueError): catalog.strict_json(b'{"a":1,"a":2}')
        for version in ('v01.0.0', 'v1.0', 'v4294967296.0.0'):
            doc = copy.deepcopy(original); doc['version'] = version
            with self.assertRaises(ValueError): catalog.validate_payload(doc)

    def test_build_verifies_assets_and_rejects_extra_and_existing_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); assets = root / 'assets'; assets.mkdir(); output = root / 'bundle'
            for role in ('normal', 'fajr'): (assets / (role + '.mp3')).write_bytes(role.encode())
            doc = catalog.build(catalog.canonical(self.reg), assets, output, False, self.private, self.public)
            self.assertEqual(catalog.verify_envelope((output / 'catalog.json').read_bytes(), self.public), doc)
            catalog.verify_assets(output, doc, 'catalog.json')
            with self.assertRaises(ValueError): catalog.build(catalog.canonical(self.reg), assets, output, False, self.private, self.public)
            (output / 'normal.mp3').write_bytes(b'bad')
            with self.assertRaises(ValueError): catalog.verify_assets(output, doc, 'catalog.json')
            (assets / 'extra').touch()
            with self.assertRaises(ValueError): catalog.build(catalog.canonical(self.reg), assets, root / 'other', True)
            self.assertFalse((root / 'other').exists())

    def test_cli_uses_committed_public_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            repo = root / 'repo'
            (repo / 'tools').mkdir(parents=True)
            (repo / 'release').mkdir()
            shutil.copyfile(catalog.ROOT / 'tools/athan_catalog.py', repo / 'tools/athan_catalog.py')
            (repo / 'release/athan-recordings.json').write_bytes(catalog.canonical(self.reg))
            public_path = repo / 'release/upgrade-public-key.pem'
            public_path.write_bytes(self.public)
            env = dict(os.environ, GIT_AUTHOR_NAME='Test', GIT_AUTHOR_EMAIL='test@example.org',
                       GIT_COMMITTER_NAME='Test', GIT_COMMITTER_EMAIL='test@example.org')
            for args in (['init', '-q'], ['add', 'release'], ['commit', '-qm', 'Approve fixtures']):
                subprocess.run(['git', '-C', str(repo), *args], check=True, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            attacker_private, attacker_public = keypair()
            public_path.write_bytes(attacker_public)
            signing_key = root / 'signing.pem'
            signing_key.write_bytes(self.private)
            assets = root / 'assets'
            assets.mkdir()
            for role in ('normal', 'fajr'):
                (assets / (role + '.mp3')).write_bytes(role.encode())
            output = root / 'bundle'
            command = [sys.executable, str(repo / 'tools/athan_catalog.py')]
            result = subprocess.run(command + ['build', '--assets', str(assets), '--output', str(output),
                                    '--signing-key', str(signing_key)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            catalog.verify_envelope((output / 'catalog.json').read_bytes(), self.public)
            result = subprocess.run(command + ['verify', '--bundle', str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            signing_key.write_bytes(attacker_private)
            rejected = root / 'rejected'
            result = subprocess.run(command + ['build', '--assets', str(assets), '--output', str(rejected),
                                    '--signing-key', str(signing_key)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Signing key differs from production P-256 trust', result.stderr)
            self.assertFalse(rejected.exists())


if __name__ == '__main__': unittest.main()

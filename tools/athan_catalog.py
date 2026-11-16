#!/usr/bin/env python3
"""Prepare or verify an offline, signed Athan catalog release bundle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / 'release/athan-recordings.json'
PUBLIC_KEY = ROOT / 'release/upgrade-public-key.pem'
FIELDS = ('id', 'role', 'name', 'bytes', 'sha256', 'file', 'attribution', 'source', 'license')
MAX_USED = 0x300000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()


def strict_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate JSON key')
            result[key] = value
        return result
    return json.loads(data, object_pairs_hook=unique)


def valid_version(version):
    return isinstance(version, str) and len(version) <= 31 and re.fullmatch(r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version) is not None and all(int(p) <= 0xffffffff for p in version[1:].split('.'))


def validate_payload(doc):
    require(isinstance(doc, dict) and set(doc) == {'schema', 'kind', 'version', 'recordings'} and type(doc['schema']) is int and doc['schema'] == 1 and doc['kind'] == 'athan_catalog' and valid_version(doc['version']), 'Invalid catalog metadata')
    records = doc['recordings']
    require(isinstance(records, list) and 2 <= len(records) <= 16, 'Invalid catalog count')
    ids, files, roles = set(), set(), {'normal': [], 'fajr': []}
    for item in records:
        require(isinstance(item, dict) and set(item) == set(FIELDS), 'Invalid recording metadata')
        ident, filename, role = item['id'], item['file'], item['role']
        require(isinstance(ident, str) and re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', ident) and len(ident) <= 48 and ident not in ids, 'Invalid or duplicate ID')
        require(isinstance(filename, str) and len(filename) <= 80 and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*\.mp3', filename) and '..' not in filename and filename not in files, 'Invalid or duplicate filename')
        require(role in roles and type(item['bytes']) is int and 0 < item['bytes'] <= MAX_USED, 'Invalid role or size')
        require(isinstance(item['sha256'], str) and re.fullmatch(r'[0-9a-f]{64}', item['sha256']), 'Invalid SHA-256')
        for key, limit in (('name', 80), ('attribution', 1024), ('source', 1024), ('license', 1024)):
            text = item[key]
            require(isinstance(text, str) and 0 < len(text.encode('utf-8')) <= limit and not any(ord(ch) < 32 or ord(ch) == 127 for ch in text), f'Invalid {key}')
        for key in ('source', 'license'):
            require(item[key].startswith('https://'), f'Invalid {key} URL')
        ids.add(ident); files.add(filename); roles[role].append(item['bytes'])
    require(all(roles.values()), 'Both recording roles are required')
    for normal in roles['normal']:
        for fajr in roles['fajr']:
            require(((4096 + normal + 15) & ~15) + fajr <= MAX_USED, 'A recording pair exceeds protected audio boundary')
    return doc


def registry_payload(data, candidate=False):
    reg = strict_json(data)
    require(isinstance(reg, dict) and set(reg) == {'schema', 'version', 'recordings'} and type(reg['schema']) is int and reg['schema'] == 1 and isinstance(reg['recordings'], list), 'Invalid recording registry')
    records = []
    for item in reg['recordings']:
        require(isinstance(item, dict) and set(FIELDS).issubset(item) and set(item).issubset(set(FIELDS) | {'approved', 'review', 'permission', 'source_sha256', 'transformation'}), 'Invalid registry entry')
        require(type(item.get('approved')) is bool and isinstance(item.get('review'), str) and bool(item['review'].strip()), 'Missing approval record')
        require(candidate or item['approved'], 'Unapproved recording in publication bundle')
        require(isinstance(item.get('permission'), str) and item['permission'].startswith('https://') and isinstance(item.get('source_sha256'), str) and re.fullmatch(r'[0-9a-f]{64}', item['source_sha256']) and isinstance(item.get('transformation'), str) and bool(item['transformation'].strip()), 'Incomplete source and permission record')
        records.append({field: item[field] for field in FIELDS})
    return validate_payload({'schema': 1, 'kind': 'athan_catalog', 'version': reg['version'], 'recordings': records})


def public_key(pem):
    key = serialization.load_pem_public_key(pem)
    require(isinstance(key, ec.EllipticCurvePublicKey) and isinstance(key.curve, ec.SECP256R1), 'Expected P-256 public key')
    return key


def sign(payload, private_pem, public_pem):
    key = serialization.load_pem_private_key(private_pem, password=None)
    pub = public_key(public_pem)
    require(isinstance(key, ec.EllipticCurvePrivateKey) and isinstance(key.curve, ec.SECP256R1) and key.public_key().public_numbers() == pub.public_numbers(), 'Signing key differs from production P-256 trust')
    envelope = canonical({'payload': payload.decode(), 'signature': key.sign(payload, ec.ECDSA(hashes.SHA256())).hex()})
    require(len(envelope) <= 32768, 'Envelope exceeds 32 KiB')
    return envelope


def verify_envelope(envelope, public_pem):
    require(0 < len(envelope) <= 32768, 'Invalid envelope size')
    wrapper = strict_json(envelope)
    require(isinstance(wrapper, dict) and set(wrapper) == {'payload', 'signature'} and isinstance(wrapper['payload'], str) and isinstance(wrapper['signature'], str) and re.fullmatch(r'[0-9a-f]{128,144}', wrapper['signature']) and len(wrapper['signature']) % 2 == 0, 'Invalid envelope')
    payload = wrapper['payload'].encode()
    require(len(payload) <= 24576, 'Payload exceeds 24 KiB')
    try:
        public_key(public_pem).verify(bytes.fromhex(wrapper['signature']), payload, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature as error:
        raise ValueError('Invalid catalog signature') from error
    doc = validate_payload(strict_json(payload))
    require(canonical(doc) == payload, 'Noncanonical catalog payload')
    return doc


def verify_assets(directory, doc, metadata_file):
    expected = {metadata_file} | {item['file'] for item in doc['recordings']}
    require({p.name for p in directory.iterdir()} == expected, 'Unexpected or missing bundle files')
    for item in doc['recordings']:
        path = directory / item['file']
        require(path.is_file() and not path.is_symlink(), 'Invalid asset file')
        require(path.stat().st_size == item['bytes'], f"Asset size mismatch: {item['file']}")
        data = path.read_bytes()
        require(len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256'], f"Asset mismatch: {item['file']}")


def build(registry, assets, output, candidate, private_pem=None, public_pem=None):
    doc = registry_payload(registry, candidate)
    payload = canonical(doc)
    require(len(payload) <= 24576, 'Payload exceeds 24 KiB')
    require(not output.resolve().is_relative_to(ROOT.resolve()) and not output.exists(), 'Output must be a new directory outside the repository')
    output.parent.mkdir(parents=True, exist_ok=True)
    require({p.name for p in assets.iterdir()} == {x['file'] for x in doc['recordings']}, 'Unexpected or missing input files')
    with tempfile.TemporaryDirectory(prefix='.athan-', dir=output.parent) as tmp:
        stage = Path(tmp) / 'bundle'; stage.mkdir()
        for item in doc['recordings']:
            source = assets / item['file']
            require(source.is_file() and not source.is_symlink(), 'Invalid input asset')
            shutil.copyfile(source, stage / item['file'])
        filename = 'catalog.payload.json' if candidate else 'catalog.json'
        (stage / filename).write_bytes(payload if candidate else sign(payload, private_pem, public_pem))
        verify_assets(stage, doc, filename)
        if not candidate:
            require(verify_envelope((stage / filename).read_bytes(), public_pem) == doc, 'Signed catalog mismatch')
        os.rename(stage, output)
    return doc


def require_committed_registry(path, data):
    require(path.resolve() == REGISTRY.resolve() and subprocess.check_output(
        ['git', 'show', 'HEAD:release/athan-recordings.json'], cwd=ROOT) == data,
        'Production requires the committed registry at HEAD')


def committed_public_key():
    return subprocess.check_output(
        ['git', 'show', 'HEAD:' + PUBLIC_KEY.relative_to(ROOT).as_posix()], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    make = sub.add_parser('build')
    make.add_argument('--registry', type=Path, default=REGISTRY)
    make.add_argument('--assets', type=Path, required=True)
    make.add_argument('--output', type=Path, required=True)
    make.add_argument('--candidate', action='store_true', help='Unsigned review bundle, including unapproved candidates')
    make.add_argument('--signing-key', type=Path)
    check = sub.add_parser('verify')
    check.add_argument('--bundle', type=Path, required=True)
    check.add_argument('--registry', type=Path, default=REGISTRY)
    check.add_argument('--candidate', action='store_true', help='Verify unsigned review bundle')
    args = parser.parse_args()
    try:
        registry = args.registry.read_bytes()
        if args.command == 'build':
            require(args.candidate != bool(args.signing_key), 'Use --candidate or --signing-key')
            registry_payload(registry, args.candidate)
            if not args.candidate:
                require_committed_registry(args.registry, registry)
                require(not args.signing_key.resolve().is_relative_to(ROOT.resolve()), 'Keep the signing key outside the repository')
            build(registry, args.assets, args.output, args.candidate,
                  args.signing_key.read_bytes() if args.signing_key else None,
                  None if args.candidate else committed_public_key())
        else:
            if not args.candidate:
                require_committed_registry(args.registry, registry)
            expected = registry_payload(registry, args.candidate)
            filename = 'catalog.payload.json' if args.candidate else 'catalog.json'
            raw = (args.bundle / filename).read_bytes()
            if args.candidate:
                require(len(raw) <= 24576 and raw == canonical(expected), 'Candidate payload mismatch')
                doc = expected
            else:
                doc = verify_envelope(raw, committed_public_key())
                require(doc == expected, 'Catalog differs from registry')
            verify_assets(args.bundle, doc, filename)
        print('Catalog and assets verified')
    except (ValueError, OSError, subprocess.CalledProcessError, TypeError, KeyError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()

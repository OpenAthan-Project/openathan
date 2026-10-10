#!/usr/bin/env python3
"""Exercise producer output with the actual pinned website validator; never upload."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from release_fixtures import ROOT, firmware
from audio_image import build_image
from release_artifacts import digest, json_bytes, make_manifest, require, PROFILES, asset_name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--website', type=Path, required=True)
    args = parser.parse_args()
    pin = json.loads((ROOT / 'release/installer-contract.json').read_text())
    head = subprocess.check_output(['git', '-C', args.website, 'rev-parse', 'HEAD'], text=True).strip()
    module = (args.website / pin['path']).resolve()
    require(head == pin['commit'] and digest(module.read_bytes()) == pin['sha256'], 'Installer contract pin differs')
    with tempfile.TemporaryDirectory(prefix='openathan-contract-test-') as temporary:
        output = Path(temporary)
        audio, _ = build_image(b'ID3-non-decodable-normal-fixture', b'ID3-non-decodable-fajr-fixture')
        for hardware, profile in PROFILES.items():
            factory, _ = firmware(profile['flash_bytes'])
            files = {asset_name(hardware, 'firmware.factory.bin'): factory, 'athan-audio.bin': audio,
                     asset_name(hardware, 'manifest.json'): json_bytes(make_manifest('a' * 40, 'v0.1.0', factory, audio, hardware))}
            for name, data in files.items():
                (output / name).write_bytes(data)
            subprocess.run([os.environ.get('NODE', 'node'), ROOT / 'tests/installer_contract.mjs', module, output, hardware], check=True)


if __name__ == '__main__':
    main()

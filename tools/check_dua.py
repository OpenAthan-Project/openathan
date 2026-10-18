#!/usr/bin/env python3
"""Execute production optional-audio downloads against deterministic host I/O."""
import argparse
import os
import re
from pathlib import Path
import subprocess
import tempfile
from audio_image import ROOT, build_image


def check(include):
    with tempfile.TemporaryDirectory(prefix='openathan-dua-host-') as temporary:
        work = Path(temporary)
        fixture = work / 'host-only.bin'
        fixture.write_bytes(build_image(b'host-only normal', b'host-only fajr')[0])
        output = work / 'dua-runtime'
        command = [os.environ.get('CXX', 'c++'), '-std=c++20', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                   '-DOPENATHAN_FIRMWARE_VERSION="v0.8.0"', '-DOPENATHAN_BUILD_COMMIT="test"',
                   '-DOPENATHAN_UPGRADE_PUBLIC_KEY="host test key"',
                   '-I' + str(ROOT / 'tests/upgrade_stubs'), '-I' + str(ROOT / 'tests/stubs'),
                   '-I' + str(include), '-I' + str(ROOT / 'lib/openathan-core/include'),
                   '-I' + str(ROOT / 'firmware'), '-I' + str(ROOT / 'firmware/esphome/components/openathan_device'),
                   str(ROOT / 'tests/dua_runtime_tests.cpp'), str(ROOT / 'firmware/esphome/components/openathan_device/dua.cpp'),
                   str(ROOT / 'lib/openathan-core/src/dua.cpp'), '-o', str(output)]
        if os.uname().sysname != 'Darwin':
            command.append('-lcrypto')
        qualification = (ROOT / 'firmware/esphome/components/openathan_upgrade_qualification/qualification.cpp').read_text()
        methods = []
        for signature in ('bool allowed_url(const std::string &url)', 'void configure_tls(esp_http_client_config_t &config)'):
            match = re.search(r'^' + re.escape(signature) + r' \{(?:[^\n]*\}\n|.*?^\}\n)', qualification, re.M | re.S)
            if not match:
                raise ValueError('Missing qualification transport: ' + signature)
            methods.append(match[0])
        (work / 'dua_qualification_transport.inc').write_text('\n'.join(methods))
        for flags in ([], ['-DOPENATHAN_PROVISIONING_TEST_STORAGE'], [
                '-DOPENATHAN_UPGRADE_QUALIFICATION', '-DOPENATHAN_PROVISIONING_TEST_STORAGE',
                '-DOPENATHAN_QUALIFICATION_ORIGIN="https://192.168.1.2:8443"',
                '-DOPENATHAN_QUALIFICATION_CA="host private CA"', '-DOPENATHAN_QUALIFICATION_STARTUP_FAILURE=false']):
            subprocess.run(command + ['-I' + str(work), *flags], check=True)
            subprocess.run([str(output), str(fixture)], check=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arduinojson', required=True, type=Path)
    check(parser.parse_args().arduinojson)

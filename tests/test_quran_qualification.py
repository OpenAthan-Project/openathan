"""Private Quran fixtures, verified TLS, profile isolation and release exclusion."""
import asyncio
import importlib.util
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_feasibility
import quran_qualification as qualification
import release_artifacts
from release_fixtures import esp_image, firmware

META = qualification.PREFIX + "www.mp3quran.net/api/v3/reciters?language=eng&reciter=1"
AUDIO = qualification.PREFIX + "cdn.mp3quran.net/qualification/001.mp3"


class QuranFixtures(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.directory = self.root / "feed"
        qualification.initialize(self.directory, "192.168.1.2")

    def tls_feed(self, mode="trusted"):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(self.directory / f"server-{mode}.pem", self.directory / "server-key.pem")
        server = qualification.tls_server(("127.0.0.1", 0), self.directory, context)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(lambda: thread.join(timeout=2))
        self.addCleanup(server.shutdown)
        return server.server_address

    def fetch(self, address, path=META, timeout=2):
        context = ssl.create_default_context(cafile=str(self.directory / "ca.pem"))
        with socket.create_connection(address, timeout=timeout) as raw:
            with context.wrap_socket(raw, server_hostname="192.168.1.2") as connection:
                connection.sendall(f"GET {path} HTTP/1.0\r\nHost: 192.168.1.2\r\n\r\n".encode())
                chunks = []
                while chunk := connection.recv(8192):
                    chunks.append(chunk)
        header, body = b"".join(chunks).split(b"\r\n\r\n", 1)
        return header, body

    def test_private_files_and_fresh_initialization(self):
        self.assertEqual(self.directory.stat().st_mode & 0o777, 0o700)
        self.assertTrue(all(path.stat().st_mode & 0o777 == 0o600 for path in self.directory.iterdir()))
        self.assertEqual(json.loads((self.directory / "control.json").read_text())["case"], "metadata-malformed")
        with self.assertRaises(FileExistsError):
            qualification.initialize(self.directory, "192.168.1.2")
        for invalid in ("127.0.0.1", "0.0.0.0", "192.0.2.1", "8.8.8.8", "localhost"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                qualification.initialize(self.root / "invalid", invalid)
        for port in (0, 443, 65536):
            with self.subTest(port=port), self.assertRaises(ValueError):
                qualification.initialize(self.root / "invalid", "192.168.1.2", port)
        self.directory.chmod(0o755)
        with self.assertRaisesRegex(ValueError, "mode 700"):
            qualification.select(self.directory, "metadata-malformed")

    def test_real_tls_verifies_ca_and_hostname(self):
        for mode in ("wrong-host", "untrusted"):
            with self.subTest(mode=mode), self.assertRaises(ssl.SSLCertVerificationError):
                self.fetch(self.tls_feed(mode))
        header, body = self.fetch(self.tls_feed())
        self.assertIn(b"200", header)
        self.assertTrue(body)

    def test_unfinished_handshake_cannot_block_other_clients(self):
        address = self.tls_feed()
        with socket.create_connection(address, timeout=2):
            header, _ = self.fetch(address)
            self.assertIn(b"200", header)

    def test_metadata_faults_and_redirect_targets(self):
        address = self.tls_feed()
        for case in qualification.CASES:
            if not case.startswith("metadata-"):
                continue
            qualification.select(self.directory, case, stall_seconds=0.05)
            header, body = self.fetch(address)
            with self.subTest(case=case):
                if case.endswith("status"):
                    self.assertIn(b"503", header); self.assertFalse(body)
                elif "redirect" in case:
                    self.assertIn(b"302", header)
                    self.assertFalse(body)
                    target = ("http://www.mp3quran.net/api/v3/reciters?language=eng&reciter=1" if case.endswith("http") else
                              "https://evil.example/forbidden" if case.endswith("host") else
                              "https://www.mp3quran.net/api/v3/reciters?language=eng&reciter=1")
                    self.assertIn(f"Location: {target}".encode(), header)
                elif case.endswith("malformed"):
                    with self.assertRaises(ValueError): json.loads(body)
                elif case.endswith("oversize"):
                    self.assertGreater(len(body), 16384)
                else:
                    declared = int(next(line.split(b": ")[1] for line in header.split(b"\r\n") if line.startswith(b"Content-Length:")))
                    self.assertLess(len(body), declared)
        requests = [json.loads(line) for line in (self.directory / "requests.jsonl").read_text().splitlines()]
        self.assertEqual(len(requests), 8)
        self.assertTrue(all(row["logical_url"].startswith("https://www.mp3quran.net/") for row in requests))

    def test_audio_faults_and_validated_media_immutability(self):
        address = self.tls_feed()
        # Host HTTP payload only; never installed, decoded or played on a device.
        media = self.root / "host-payload.mp3"
        media.write_bytes(b"host transport fixture" * 100)
        control = dict(media=str(media), media_sha256=qualification.hashlib.sha256(media.read_bytes()).hexdigest())
        with patch.object(qualification, "validate_media", return_value=control):
            for case in ("normal", "audio-truncate", "audio-stall"):
                qualification.select(self.directory, case, media, stall_seconds=0.05)
                header, body = self.fetch(address, AUDIO)
                self.assertIn(b"Content-Type: audio/mpeg", header)
                if case == "normal": self.assertEqual(body, media.read_bytes())
                else: self.assertLess(len(body), media.stat().st_size)
            media.write_bytes(b"changed after staging")
            with self.assertRaises(ValueError): self.fetch(address, AUDIO)
        for case in ("audio-status", "audio-content-type", "audio-decode", "audio-redirect-http", "audio-redirect-host", "audio-redirect-loop"):
            qualification.select(self.directory, case)
            header, body = self.fetch(address, AUDIO)
            if case.endswith("status"):
                self.assertIn(b"503", header); self.assertFalse(body)
            elif "redirect" in case:
                self.assertIn(b"302", header); self.assertFalse(body)
            elif case.endswith("content-type"): self.assertIn(b"Content-Type: text/plain", header)
            else: self.assertIn(b"No audio frames", body)

    def test_stall_really_leaves_the_body_incomplete(self):
        qualification.select(self.directory, "metadata-stall", stall_seconds=0.5)
        with self.assertRaises(TimeoutError):
            self.fetch(self.tls_feed(), timeout=0.15)

    def test_selection_requires_real_media_and_valid_duration(self):
        for case in qualification.MEDIA_CASES:
            with self.subTest(case=case), self.assertRaisesRegex(ValueError, "approved real MP3"):
                qualification.select(self.directory, case)
        for duration in (-1, 46, float("nan"), float("inf")):
            with self.subTest(duration=duration), self.assertRaises(ValueError):
                qualification.select(self.directory, "metadata-malformed", stall_seconds=duration)
        invalid = self.root / "not-a-recording.mp3"
        invalid.write_bytes(b"deliberately invalid, no MP3 frames")
        with self.assertRaises(subprocess.CalledProcessError):
            qualification.validate_media(invalid)
        with patch.object(qualification.subprocess, "run") as decode:
            qualification.validate_media(invalid)
            arguments = decode.call_args.args[0]
            self.assertEqual(arguments[arguments.index("-i")-2:arguments.index("-i")], ["-f", "mp3"])
            self.assertTrue(decode.call_args.kwargs["check"])
        link = self.root / "link.mp3"; link.symlink_to(invalid)
        with self.assertRaises(ValueError): qualification.validate_media(link)
        with self.assertRaises(ValueError): qualification.validate_media(ROOT / "recording.mp3")


class QuranQualificationProfiles(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.directory = self.root / "feed"
        qualification.initialize(self.directory, "192.168.1.2")
        path = ROOT / "firmware/esphome/components/openathan_quran_qualification/__init__.py"
        spec = importlib.util.spec_from_file_location("quran_qualification_config", path)
        self.module = importlib.util.module_from_spec(spec); spec.loader.exec_module(self.module)

    def test_lan_origin_and_profile_validation(self):
        self.assertEqual(self.module.origin("https://192.168.1.2:8443"), "https://192.168.1.2:8443")
        for invalid in ("http://192.168.1.2:8443", "https://github.com:8443", "https://127.0.0.1:8443",
                        "https://192.0.2.1:8443", "https://192.168.1.2:8443/", "https://x@192.168.1.2:8443",
                        "https://192.168.1.2:8443?", "https://192.168.1.2:08443", "https://10.0.0.1:443", None):
            with self.subTest(invalid=invalid), self.assertRaises(self.module.cv.Invalid): self.module.origin(invalid)
        valid = dict(esphome=dict(name="openathan-test", name_add_mac_suffix=True),
                     openathan_device=dict(updates_enabled=False), openathan_provisioning_validation={})
        with patch.object(self.module.fv, "full_config") as full:
            full.get.return_value = valid
            self.assertEqual(self.module.validate({}), {})
            for override in (dict(esphome=dict(name="openathan", name_add_mac_suffix=True)),
                             dict(esphome=dict(name="openathan-test", name_add_mac_suffix=False)),
                             dict(openathan_device=dict(updates_enabled=True)),
                             dict(openathan_device={}), dict(openathan_upgrade_qualification={})):
                full.get.return_value = {**valid, **override}
                with self.assertRaises(self.module.cv.Invalid): self.module.validate({})
            full.get.return_value = {key: value for key, value in valid.items() if key != "openathan_provisioning_validation"}
            with self.assertRaises(self.module.cv.Invalid): self.module.validate({})

    def test_ca_validation_embeds_only_one_public_ca(self):
        config = dict(origin="https://192.168.1.2:8443", ca_file=str(self.directory / "ca.pem"))
        with patch.object(self.module.cg, "add_define") as define:
            asyncio.run(self.module.to_code(config))
            self.assertEqual(define.call_count, 3)
        for data in ((self.directory / "server-trusted.pem").read_bytes(), b"not a certificate",
                     (self.directory / "ca-key.pem").read_bytes(),
                     (self.directory / "ca.pem").read_bytes() * 2, b"x" * 4097):
            invalid = self.root / "invalid.pem"; invalid.write_bytes(data)
            with self.assertRaises(self.module.cv.Invalid):
                asyncio.run(self.module.to_code({**config, "ca_file": str(invalid)}))

    def profile(self, defines, compiled=True, source=True):
        build = self.root / "build"
        (build / "src/esphome/core").mkdir(parents=True, exist_ok=True)
        (build / "src/esphome/core/defines.h").write_text(defines)
        (build / "build").mkdir(exist_ok=True)
        component = build / "src/esphome/components/openathan_quran_qualification"
        if source: component.mkdir(parents=True, exist_ok=True)
        elif component.exists(): component.rmdir()
        (build / "build/compile_commands.json").write_text(
            "/openathan_quran_qualification/qualification.cpp" if compiled else "[]")
        return build

    def test_ordinary_builds_exclude_qualification_definitions_and_code(self):
        app = firmware()[1]
        self.assertEqual(check_feasibility.inspect_build_profile(
            self.profile("", compiled=False, source=False), app), (False, False))
        for defines in ('#define OPENATHAN_QURAN_QUALIFICATION_CA "test CA"\n',
                        '#define OPENATHAN_QURAN_QUALIFICATION 1\n',
                        '#define USE_OPENATHAN_QURAN_QUALIFICATION\n'):
            with self.subTest(defines=defines), self.assertRaisesRegex(ValueError, "exclude"):
                check_feasibility.inspect_build_profile(self.profile(defines, compiled=False, source=False), app)
        with self.assertRaisesRegex(ValueError, "exclude"):
            check_feasibility.inspect_build_profile(self.profile("", source=False), app)
        with self.assertRaisesRegex(ValueError, "exclude"):
            check_feasibility.inspect_build_profile(self.profile("", compiled=False), app)

    def test_capacity_profile_and_release_exclusion(self):
        app = esp_image(b"x" * 128 + qualification.MARKER.encode())
        defines = '#define OPENATHAN_QURAN_QUALIFICATION\n#define OPENATHAN_PROVISIONING_TEST_STORAGE\n#define OPENATHAN_UPDATES_ENABLED 0\n'
        self.assertEqual(check_feasibility.inspect_build_profile(self.profile(defines), app), (True, True))
        for invalid in (defines.replace('OPENATHAN_PROVISIONING_TEST_STORAGE', 'OTHER'),
                        defines.replace('UPDATES_ENABLED 0', 'UPDATES_ENABLED 1'),
                        defines + '#define OPENATHAN_UPGRADE_QUALIFICATION\n',
                        defines.replace('#define OPENATHAN_QURAN_QUALIFICATION\n', '')):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                check_feasibility.inspect_build_profile(self.profile(invalid), app)
        with self.assertRaisesRegex(ValueError, "separate component"):
            check_feasibility.inspect_build_profile(self.profile(defines, compiled=False), app)
        with self.assertRaisesRegex(ValueError, "identity marker"):
            check_feasibility.inspect_build_profile(self.profile(defines), firmware()[1])
        factory, _ = firmware()
        with self.assertRaisesRegex(ValueError, "Qualification firmware"):
            release_artifacts.validate_firmware_images(factory[:0x10000] + app, app)

    def test_real_profile_generation_and_production_exclusion(self):
        config = self.root / "qualification.yaml"
        config.write_text(f"packages:\n  product: !include {ROOT}/firmware/esphome/waveshare/quran-qualification.yaml\n")
        (self.root / "secrets.yaml").write_text(
            'diagnostic_api_key: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAE=\n'
            'quran_qualification_origin: https://192.168.1.2:8443\n'
            f'quran_qualification_ca_file: {self.directory / "ca.pem"}\n')
        build = self.root / "generated"
        def generate():
            result = subprocess.run([sys.executable, "-m", "esphome", "compile", "--only-generate", str(config)],
                                    env={**os.environ, "ESPHOME_BUILD_PATH": str(build)}, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        generate()
        generated = build / "openathan-test"
        defines = (generated / "src/esphome/core/defines.h").read_text()
        self.assertIn('#define OPENATHAN_QURAN_QUALIFICATION\n', defines)
        self.assertIn('#define OPENATHAN_UPDATES_ENABLED 0\n', defines)
        self.assertIn('#define OPENATHAN_PROVISIONING_TEST_STORAGE\n', defines)
        self.assertTrue((generated / "src/esphome/components/openathan_quran_qualification/qualification.cpp").is_file())
        config.write_text(f"packages:\n  product: !include {ROOT}/firmware/esphome/waveshare/production.yaml\n")
        generate()
        production = build / "openathan"
        self.assertNotIn('OPENATHAN_QURAN_QUALIFICATION', (production / 'src/esphome/core/defines.h').read_text())
        self.assertFalse((production / 'src/esphome/components/openathan_quran_qualification').exists())


if __name__ == "__main__":
    unittest.main()

"""Qualification isolation, TLS/feed failures and release exclusion."""
import importlib.util
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import ssl
import struct
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.request import build_opener

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import upgrade_qualification as qualification
import release_artifacts
import check_feasibility
from release_fixtures import esp_image, firmware


class QualificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name) / "feed"
        qualification.initialize(self.directory, "192.168.1.2")
        payload = bytearray(256)
        struct.pack_into("<I", payload, 0, 0xabcd5432)
        payload[16:22] = b"v0.0.2"
        payload[128:128+len(qualification.MARKER)] = qualification.MARKER
        self.app = esp_image(bytes(payload))
        self.application = Path(self.temporary.name) / "qualification.bin"
        self.application.write_bytes(self.app)

    def test_private_material_and_immutable_candidates(self):
        self.assertEqual(self.directory.stat().st_mode & 0o777, 0o700)
        self.assertTrue(all(path.stat().st_mode & 0o777 == 0o600 for path in self.directory.iterdir()))
        qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)
        with self.assertRaises(FileExistsError):
            qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)
        with self.assertRaises(ValueError):
            qualification.stage(self.directory, self.application, "v0.0.4", "a"*40)
        with self.assertRaises(ValueError):
            qualification.offer(self.directory, "v0.0.1")

    def test_production_images_cannot_enter_test_feed(self):
        self.application.write_bytes(firmware()[1])
        with self.assertRaisesRegex(ValueError, "qualification application"):
            qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)

    def test_legacy_feed_without_hardware_retains_atom_paths(self):
        path = self.directory / "feed.json"
        config = json.loads(path.read_text())
        del config["hardware"]
        path.write_text(json.dumps(config))
        address = self.tls_feed()
        headers, _ = self.fetch(address, "/releases/latest/download/upgrade.json")
        self.assertIn(b"200", headers)

    def test_waveshare_feed_binds_hardware_flash_size_and_asset_paths(self):
        original_app = self.app
        base = Path(self.temporary.name)
        from upgrade_artifacts import validate
        for hardware in (release_artifacts.AUDIO_BOARD, release_artifacts.WAVESHARE):
            with self.subTest(hardware=hardware):
                self.directory = base / hardware
                qualification.initialize(self.directory, "192.168.1.2", hardware=hardware)
                self.application.write_bytes(esp_image(original_app[32:288] + hardware.encode() + b'\0'))
                with self.assertRaisesRegex(ValueError, "16 MiB"):
                    qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)
                self.application.write_bytes(esp_image(original_app[32:288] + b'wrong-board\0', flash_bytes=0x1000000))
                with self.assertRaisesRegex(ValueError, "hardware differs"):
                    qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)
                self.app = esp_image(original_app[32:288] + hardware.encode() + b'\0', flash_bytes=0x1000000)
                self.application.write_bytes(self.app)
                address = self.tls_feed()
                descriptor = release_artifacts.asset_name(hardware, "upgrade.json")
                application = release_artifacts.asset_name(hardware, "firmware.ota.bin")
                headers, envelope = self.fetch(address, f"/releases/latest/download/{descriptor}")
                self.assertIn(b"200", headers)
                validate(envelope, self.app, (self.directory / "public-key.pem").read_bytes(),
                         "v0.0.2", "a"*40, hardware)
                headers, app = self.fetch(address, f"/releases/download/v0.0.2/{application}")
                self.assertIn(b"200", headers)
                self.assertEqual(app, self.app)
                for wrong in ("upgrade.json", "firmware.ota.bin", "waveshare-other.upgrade.json"):
                    headers, _ = self.fetch(address, f"/releases/latest/download/{wrong}")
                    self.assertIn(b"404", headers)

    def test_qualification_images_cannot_enter_release_bundles(self):
        factory, _ = firmware()
        with self.assertRaisesRegex(ValueError, "Qualification firmware"):
            release_artifacts.validate_firmware_images(factory[:0x10000] + self.app, self.app)

    def test_release_exclusion_cannot_depend_on_only_one_marker(self):
        factory, _ = firmware()
        for marker in (*release_artifacts.QUALIFICATION_MARKERS, *release_artifacts.ISOLATED_STORAGE_MARKERS):
            payload = bytearray(256)
            payload[128:128+len(marker)] = marker
            app = esp_image(bytes(payload))
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, "test firmware|Qualification firmware"):
                release_artifacts.validate_firmware_images(factory[:0x10000] + app, app)

    def build_profile(self, defines="", compiled=False, source=False):
        build = Path(self.temporary.name) / "build"
        (build / "src/esphome/core").mkdir(parents=True, exist_ok=True)
        (build / "src/esphome/core/defines.h").write_text(defines)
        (build / "build").mkdir(exist_ok=True)
        (build / "build/compile_commands.json").write_text(
            "/src/esphome/components/openathan_upgrade_qualification/qualification.cpp" if compiled else "[]")
        if source:
            (build / "src/esphome/components/openathan_upgrade_qualification").mkdir(parents=True, exist_ok=True)
        return build

    def test_production_profile_rejects_test_code_and_definitions(self):
        app = firmware()[1]
        build = self.build_profile()
        self.assertEqual(check_feasibility.inspect_build_profile(build, app), (False, False))
        for defines in ('#define OPENATHAN_QUALIFICATION_CA "test CA"\n',
                        '#define OPENATHAN_UPGRADE_QUALIFICATION 1\n',
                        '#define USE_OPENATHAN_UPGRADE_QUALIFICATION\n'):
            with self.subTest(defines=defines), self.assertRaisesRegex(ValueError, "exclude"):
                check_feasibility.inspect_build_profile(self.build_profile(defines), app)
        with self.assertRaisesRegex(ValueError, "exclude"):
            check_feasibility.inspect_build_profile(self.build_profile(compiled=True), app)
        with self.assertRaisesRegex(ValueError, "exclude"):
            check_feasibility.inspect_build_profile(self.build_profile(source=True), app)

    def test_qualification_profile_requires_storage_and_separate_component(self):
        defines = '#define OPENATHAN_UPGRADE_QUALIFICATION\n'
        with self.assertRaisesRegex(ValueError, "isolated storage"):
            check_feasibility.inspect_build_profile(self.build_profile(defines), self.app)
        defines += '#define OPENATHAN_PROVISIONING_TEST_STORAGE\n'
        with self.assertRaisesRegex(ValueError, "separate component"):
            check_feasibility.inspect_build_profile(self.build_profile(defines), self.app)
        self.assertEqual(check_feasibility.inspect_build_profile(
            self.build_profile(defines, compiled=True, source=True), self.app), (True, True))

    def test_test_storage_is_capacity_only_and_cannot_be_released(self):
        payload = bytearray(256)
        marker = b'oa_setup_test\0'
        payload[128:128+len(marker)] = marker
        app = esp_image(bytes(payload))
        defines = '#define OPENATHAN_PROVISIONING_TEST_STORAGE\n'
        self.assertEqual(check_feasibility.inspect_build_profile(self.build_profile(defines), app), (False, True))
        with self.assertRaisesRegex(ValueError, "Isolated test firmware"):
            release_artifacts.reject_test_material(app)

    def test_lan_origin_validation(self):
        path = ROOT / "firmware/esphome/components/openathan_upgrade_qualification/__init__.py"
        spec = importlib.util.spec_from_file_location("qualification_config", path)
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        self.assertEqual(module.origin("https://192.168.1.2:8443"), "https://192.168.1.2:8443")
        for invalid in ("http://192.168.1.2:8443", "https://github.com:8443", "https://127.0.0.1:8443",
                        "https://192.0.2.1:8443", "https://192.168.1.2:8443/", "https://x@192.168.1.2:8443",
                        "https://@192.168.1.2:8443", "https://192.168.1.2:8443?", "https://192.168.1.2:08443"):
            with self.assertRaises(module.cv.Invalid):
                module.origin(invalid)
        with patch.object(module.fv, "full_config") as full:
            full.get.return_value = {"esphome": {"name": "openathan", "name_add_mac_suffix": True}}
            with self.assertRaises(module.cv.Invalid):
                module.validate(dict(version="v0.0.1", startup_failure=False))

    def test_observer_controls_send_same_origin_json(self):
        posts = []
        origin = ""

        class Device(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                data = json.dumps({"qualification": {"identity": qualification.MARKER.decode()}}).encode()
                self.send_response(200)
                self.end_headers()
                self.wfile.write(data)

            def do_POST(self):
                if (self.headers.get("Origin") != origin or
                        self.headers.get("Content-Type") != "application/json"):
                    self.send_error(403, "Same-origin JSON required")
                    return
                posts.append((self.path, json.loads(self.rfile.read(int(self.headers["Content-Length"])))))
                self.do_GET()

        server = ThreadingHTTPServer(("127.0.0.1", 0), Device)
        origin = f"http://127.0.0.1:{server.server_port}"
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(lambda: thread.join(timeout=2))
        self.addCleanup(server.shutdown)
        with patch.object(qualification, "device_client", return_value=(origin, build_opener())), \
                patch.object(qualification.getpass, "getpass", return_value="test-only password"):
            qualification.observe("192.168.1.2", Path(self.temporary.name) / "observation.jsonl", 1,
                                  command="arm", phase="before_boot_selection")
        self.assertEqual(posts, [("/api/firmware/qualification",
                                 {"command": "arm", "phase": "before_boot_selection"})])

    def tls_feed(self):
        qualification.stage(self.directory, self.application, "v0.0.2", "a"*40)
        qualification.offer(self.directory, "v0.0.2")
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(self.directory / "server.pem", self.directory / "server-key.pem")
        server = qualification.tls_server(("127.0.0.1", 0), self.directory, context)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        self.addCleanup(server.server_close); self.addCleanup(lambda: thread.join(timeout=2)); self.addCleanup(server.shutdown)
        return server.server_address

    def fetch(self, address, path, context=None, hostname="192.168.1.2", timeout=5):
        context = context or ssl.create_default_context(cafile=str(self.directory / "ca.pem"))
        with socket.create_connection(address, timeout=timeout) as raw:
            with context.wrap_socket(raw, server_hostname=hostname) as connection:
                connection.sendall(f"GET {path} HTTP/1.0\r\nHost: {hostname}\r\n\r\n".encode())
                chunks = []
                while chunk := connection.recv(8192):
                    chunks.append(chunk)
        header, body = b"".join(chunks).split(b"\r\n\r\n", 1)
        return header, body

    def test_unfinished_handshake_cannot_block_other_clients(self):
        address = self.tls_feed()
        with socket.create_connection(address, timeout=2):
            header, body = self.fetch(address, "/releases/latest/download/upgrade.json", timeout=2)
            self.assertIn(b"200", header)
            self.assertTrue(body)

    def test_real_tls_and_served_faults(self):
        address = self.tls_feed()
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.fetch(address, "/releases/latest/download/upgrade.json", ssl.create_default_context())
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.fetch(address, "/releases/latest/download/upgrade.json", hostname="wrong.local")
        header, envelope = self.fetch(address, "/releases/latest/download/upgrade.json")
        self.assertIn(b"200", header)
        from upgrade_artifacts import validate
        public = (self.directory / "public-key.pem").read_bytes()
        validate(envelope, self.app, public, "v0.0.2", "a"*40)
        for fault in qualification.FAULTS:
            qualification.offer(self.directory, "v0.0.2", fault)
            if fault == "signature":
                _, data = self.fetch(address, "/releases/latest/download/upgrade.json")
                with self.assertRaises(ValueError):
                    validate(data, self.app, public, "v0.0.2", "a"*40)
            else:
                headers, data = self.fetch(address, "/releases/download/v0.0.2/firmware.ota.bin")
                self.assertIn(f"Content-Length: {len(self.app)}".encode(), headers)
                if fault == "none": self.assertEqual(data, self.app)
                elif fault == "corrupt": self.assertNotEqual(data, self.app)
                else: self.assertEqual(len(data), len(self.app)//2)
        for path in ("/signing-key.pem", "/ca-key.pem", "/releases/download/../signing-key.pem", "/"):
            headers, _ = self.fetch(address, path)
            self.assertIn(b"404", headers)


if __name__ == "__main__":
    unittest.main()

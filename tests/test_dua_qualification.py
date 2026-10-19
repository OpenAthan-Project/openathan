"""Host-only qualification fixtures and isolation; never install synthetic media."""

import asyncio
import importlib.util
import json
import os
from pathlib import Path
import socket
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import audio_image
import check_feasibility
import dua_media
import dua_qualification as qualification
import release_artifacts as artifacts
from release_fixtures import esp_image, firmware


class DuaQualificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.directory = self.root / "feed"
        qualification.initialize(self.directory, "192.168.1.2")
        path = (
            ROOT / "firmware/esphome/components/openathan_dua_qualification/__init__.py"
        )
        spec = importlib.util.spec_from_file_location("dua_qualification_config", path)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.payloads = tuple(
            b"HOST-ONLY-NOT-PLAYABLE-" + role.encode()
            for role in ("normal", "fajr", "dua")
        )
        self.registry = dict(
            schema=1,
            tracks=[
                dict(
                    role=role,
                    sha256=artifacts.digest(data),
                    approved=True,
                    source="https://example.org/audio",
                    license="https://example.org/license",
                    attribution="Synthetic fixture",
                )
                for role, data in zip(("normal", "fajr", "dua"), self.payloads)
            ],
        )
        self.sources = {
            artifacts.MEDIA_REGISTRY: artifacts.json_bytes(self.registry),
            artifacts.LICENSE_PATH: "\n".join(
                t["sha256"] for t in self.registry["tracks"]
            ).encode(),
            "release/upgrade-public-key.pem": (
                ROOT / "release/upgrade-public-key.pem"
            ).read_bytes(),
        }
        self.paths = []
        for role, data in zip(("normal", "fajr", "dua"), self.payloads):
            target = self.root / (role + ".mp3")
            target.write_bytes(data)
            self.paths.append(target)

    def stage(self):
        with (
            patch.object(
                qualification.release,
                "source_file",
                side_effect=lambda commit, name: self.sources[name],
            ),
            patch.object(qualification.release, "validate_dua_recording"),
        ):
            return qualification.stage(self.directory, "a" * 40, *self.paths)

    def test_staging_is_immutable_approved_and_preserves_athan(self):
        record = self.stage()
        folder = self.directory / qualification.VERSION
        self.assertEqual((folder / "dua.mp3").read_bytes(), self.payloads[2])
        self.assertEqual(
            record["recordings"][-1]["sha256"], artifacts.digest(self.payloads[2])
        )
        baseline = (folder / "baseline-audio.bin").read_bytes()
        extended = (folder / "fresh-install-private-audio.bin").read_bytes()
        self.assertEqual(
            baseline[: dua_media.BANK_START], extended[: dua_media.BANK_START]
        )
        self.assertEqual(
            artifacts.audio_payloads(
                extended, (self.directory / "public-key.pem").read_bytes()
            ),
            list(self.payloads),
        )
        self.assertEqual(self.directory.stat().st_mode & 0o777, 0o700)
        self.assertTrue(
            all(p.stat().st_mode & 0o777 == 0o600 for p in folder.iterdir())
        )
        with self.assertRaises(FileExistsError):
            self.stage()

    def test_unapproved_or_changed_recording_cannot_be_staged(self):
        self.paths[2].write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "approved hash"):
            self.stage()
        self.paths[2].write_bytes(self.payloads[2])
        self.registry["tracks"][2]["approved"] = False
        self.sources[artifacts.MEDIA_REGISTRY] = artifacts.json_bytes(self.registry)
        with self.assertRaisesRegex(ValueError, "not been approved"):
            self.stage()

    def test_only_private_origin_and_disabled_update_profile(self):
        self.assertEqual(
            self.module.origin("https://192.168.1.2:8443"), "https://192.168.1.2:8443"
        )
        for invalid in (
            "http://192.168.1.2:8443",
            "https://127.0.0.1:8443",
            "https://github.com:8443",
            "https://192.168.1.2:8443/",
            "https://x@192.168.1.2:8443",
            "https://192.168.1.2:8443?",
            "https://192.168.1.2:08443",
        ):
            with (
                self.subTest(origin=invalid),
                self.assertRaises(self.module.cv.Invalid),
            ):
                self.module.origin(invalid)
        config = dict(version="v0.0.1")
        valid = dict(
            openathan_provisioning_validation={},
            openathan_device=dict(updates_enabled=False),
            esphome=dict(
                name="openathan-test",
                name_add_mac_suffix=True,
                project=dict(version="v0.0.1"),
            ),
        )
        with patch.object(self.module.fv, "full_config") as full:
            full.get.return_value = valid
            self.assertEqual(self.module.validate(config), config)
            for override in (
                dict(openathan_device=dict(updates_enabled=True)),
                dict(openathan_device={}),
                dict(openathan_upgrade_qualification={}),
                dict(openathan_quran_qualification={}),
                dict(esphome=dict(name="openathan", name_add_mac_suffix=True)),
            ):
                full.get.return_value = {**valid, **override}
                with self.assertRaises(self.module.cv.Invalid):
                    self.module.validate(config)
            full.get.return_value = {
                k: v
                for k, v in valid.items()
                if k != "openathan_provisioning_validation"
            }
            with self.assertRaises(self.module.cv.Invalid):
                self.module.validate(config)

    def test_embedded_trust_rejects_private_or_production_keys(self):
        config = dict(
            origin="https://192.168.1.2:8443",
            ca_file=str(self.directory / "ca.pem"),
            public_key_file=str(self.directory / "public-key.pem"),
        )
        with patch.object(self.module.cg, "add_define") as define:
            asyncio.run(self.module.to_code(config))
            self.assertEqual(define.call_count, 4)
        for name in ("signing-key.pem", "server.pem", "ca-key.pem"):
            with self.assertRaises(self.module.cv.Invalid):
                asyncio.run(
                    self.module.to_code(
                        {**config, "public_key_file": str(self.directory / name)}
                    )
                )
        with self.assertRaises(self.module.cv.Invalid):
            asyncio.run(
                self.module.to_code(
                    {
                        **config,
                        "public_key_file": str(ROOT / "release/upgrade-public-key.pem"),
                    }
                )
            )
        with self.assertRaises(self.module.cv.Invalid):
            asyncio.run(
                self.module.to_code(
                    {**config, "ca_file": str(self.directory / "server.pem")}
                )
            )

    def test_capacity_and_release_guards(self):
        build = self.root / "build"
        (build / "src/esphome/core").mkdir(parents=True)
        component = build / "src/esphome/components/openathan_dua_qualification"
        component.mkdir(parents=True)
        (build / "build").mkdir()
        commands = build / "build/compile_commands.json"
        commands.write_text("/openathan_dua_qualification/qualification.cpp")
        definitions = build / "src/esphome/core/defines.h"
        valid = "#define OPENATHAN_DUA_QUALIFICATION\n#define OPENATHAN_PROVISIONING_TEST_STORAGE\n#define OPENATHAN_UPDATES_ENABLED 0\n"
        app = esp_image(qualification.MARKER.encode() + b"x" * 128)
        definitions.write_text(valid)
        self.assertEqual(
            check_feasibility.inspect_build_profile(build, app), (True, True)
        )
        for invalid in (
            valid.replace("UPDATES_ENABLED 0", "UPDATES_ENABLED 1"),
            valid.replace("OPENATHAN_PROVISIONING_TEST_STORAGE", "OTHER"),
            valid + "#define OPENATHAN_UPGRADE_QUALIFICATION\n",
            valid.replace("#define OPENATHAN_DUA_QUALIFICATION\n", ""),
        ):
            definitions.write_text(invalid)
            with self.assertRaises(ValueError):
                check_feasibility.inspect_build_profile(build, app)
        definitions.write_text(valid)
        commands.write_text("[]")
        with self.assertRaisesRegex(ValueError, "separate component"):
            check_feasibility.inspect_build_profile(build, app)
        commands.write_text("/openathan_dua_qualification/qualification.cpp")
        definitions.write_text("")
        with self.assertRaises(ValueError):
            check_feasibility.inspect_build_profile(build, firmware()[1])
        with self.assertRaisesRegex(ValueError, "Qualification firmware"):
            artifacts.reject_test_material(app)

    def tls_feed(self):
        self.stage()
        server = qualification.server(self.directory, "127.0.0.1", 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(lambda: thread.join(timeout=2))
        self.addCleanup(server.shutdown)
        return server.server_address

    def fetch(self, address, path, hostname="192.168.1.2"):
        context = ssl.create_default_context(cafile=str(self.directory / "ca.pem"))
        with socket.create_connection(address, timeout=3) as raw:
            with context.wrap_socket(raw, server_hostname=hostname) as connection:
                connection.sendall(
                    f"GET {path} HTTP/1.0\r\nHost: {hostname}\r\n\r\n".encode()
                )
                data = b""
                while chunk := connection.recv(8192):
                    data += chunk
        return data.split(b"\r\n\r\n", 1)

    def test_tls_feed_only_exposes_selected_version_media_and_faults(self):
        address = self.tls_feed()
        prefix = f"/releases/download/{qualification.VERSION}/"
        header, body = self.fetch(address, prefix + "dua.mp3")
        self.assertIn(b"200 OK", header)
        self.assertEqual(body, self.payloads[2])
        _, envelope = self.fetch(address, prefix + "dua.json")
        dua_media.validate(
            envelope,
            body,
            (self.directory / "public-key.pem").read_bytes(),
            qualification.VERSION,
        )
        for path in (
            "/signing-key.pem",
            "/" + qualification.VERSION + "/dua.mp3",
            prefix + "../signing-key.pem",
            "/releases/download/v0.0.2/dua.mp3",
            prefix + "dua.mp3?other",
        ):
            self.assertIn(b"404", self.fetch(address, path)[0])
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.fetch(address, prefix + "dua.mp3", "wrong.example")
        for fault in qualification.FAULTS:
            qualification.offer(self.directory, fault)
            _, descriptor = self.fetch(address, prefix + "dua.json")
            _, audio = self.fetch(address, prefix + "dua.mp3")
            if fault in ("none", "stall"):
                dua_media.validate(
                    descriptor, audio, (self.directory / "public-key.pem").read_bytes()
                )
            else:
                with self.assertRaises(Exception):
                    dua_media.validate(
                        descriptor,
                        audio,
                        (self.directory / "public-key.pem").read_bytes(),
                    )

    def test_real_profiles_and_production_exclusion(self):
        source = self.root / "source"
        tracked = subprocess.check_output(
            [
                "git",
                "ls-files",
                "--cached",
                "--others",
                "--exclude-standard",
                "--",
                "firmware",
                "lib",
                "release",
                "third_party",
                "web/device-ui",
            ],
            cwd=ROOT,
            text=True,
        ).splitlines()
        for relative in tracked:
            original = ROOT / relative
            if original.is_file():
                target = source / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(original, target)
        config = self.root / "qualification.yaml"
        (self.root / "secrets.yaml").write_text(
            "dua_qualification_origin: https://192.168.1.2:8443\n"
            f"dua_qualification_ca_file: {self.directory / 'ca.pem'}\n"
            f"dua_qualification_public_key_file: {self.directory / 'public-key.pem'}\n"
        )
        build = self.root / "generated"
        for profile in (
            "dua/qualification",
            "waveshare/dua-qualification",
            "openathan",
            "waveshare/production",
        ):
            config.write_text(
                f"packages:\n  product: !include {source}/firmware/esphome/{profile}.yaml\n"
            )
            run = subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "esphome",
                    "compile",
                    "--only-generate",
                    str(config),
                ],
                env={**os.environ, "ESPHOME_BUILD_PATH": str(build)},
                capture_output=True,
                text=True,
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            private = "qualification" in profile
            generated = build / ("openathan-test" if private else "openathan")
            defines = (generated / "src/esphome/core/defines.h").read_text()
            self.assertEqual("OPENATHAN_DUA_QUALIFICATION\n" in defines, private)
            self.assertEqual(
                (
                    generated / "src/esphome/components/openathan_dua_qualification"
                ).exists(),
                private,
            )
            if private:
                self.assertIn("#define OPENATHAN_UPDATES_ENABLED 0\n", defines)
                self.assertIn("#define OPENATHAN_PROVISIONING_TEST_STORAGE\n", defines)
                self.assertIn('#define OPENATHAN_FIRMWARE_VERSION "v0.0.1"\n', defines)


if __name__ == "__main__":
    unittest.main()

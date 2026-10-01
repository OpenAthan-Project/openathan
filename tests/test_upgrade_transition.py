"""USB planning and fresh-state guards; no physical device is accessed."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from unittest.mock import MagicMock
from types import SimpleNamespace
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import upgrade_transition as transition
from release_artifacts import digest, json_bytes, ISOLATED_STORAGE_MARKERS, QUALIFICATION_MARKERS
from release_fixtures import esp_image
from esptool.loader import ESPLoader
from esptool.util import FatalError
from serial import SerialException


def production_app(extra=b""):
    payload = bytearray(256)
    struct.pack_into("<I", payload, 0, 0xabcd5432)
    payload[16:22] = b"v0.2.0"
    return esp_image(bytes(payload) + extra)


def metadata(sequence=1, state=2):
    data = bytearray(b"\xff"*0x2000)
    struct.pack_into("<I", data, 0, sequence)
    struct.pack_into("<II", data, 24, state, zlib.crc32(struct.pack("<I", sequence), 0xffffffff))
    return bytes(data)


class TransitionTests(unittest.TestCase):
    def usb_fixture(self, app):
        # Run the real adapter and esptool write/verify paths with no serial port.
        esp = MagicMock()
        esp.CHIP_NAME = "ESP32-S3"
        esp.IMAGE_CHIP_ID = 9
        esp.IS_STUB = True
        esp.secure_download_mode = False
        esp.WRITE_FLASH_ATTEMPTS = ESPLoader.WRITE_FLASH_ATTEMPTS
        esp.FLASH_WRITE_SIZE = 16384
        esp.FLASH_SECTOR_SIZE = 4096
        esp.read_mac.return_value = bytes.fromhex("aabbccddeeff")
        esp.get_security_info.return_value = dict(flags=0, flash_crypt_cnt=0)
        esp.get_secure_boot_v1_enabled.return_value = False
        esp.get_secure_boot_enabled.return_value = False
        esp.get_flash_encryption_enabled.return_value = False
        esp.get_encrypted_download_disabled.return_value = False
        esp.get_major_chip_version.return_value = 0
        esp.flash_id.return_value = 0x1740c8
        esp.flash_md5sum.return_value = hashlib.md5(app).hexdigest()
        def stub():
            esp.IS_STUB = True
            return esp
        esp.run_stub.side_effect = stub
        with patch("esptool.get_default_connected_device", return_value=esp), patch("esptool.cmds.attach_flash"):
            device = transition.Device("/dev/cu.test", "aa:bb:cc:dd:ee:ff")
        self.addCleanup(device.close)
        return device, esp

    def test_real_esptool_disconnect_stops_without_reconnect_or_reflash(self):
        app = production_app()
        for address in (0, 0x10000):
            for method in ("flash_defl_begin", "flash_defl_block"):
                with self.subTest(address=address, method=method):
                    device, esp = self.usb_fixture(app)
                    operation = getattr(esp, method)
                    def disconnect_once(*args, **kwargs):
                        if operation.call_count == 1:
                            raise SerialException("injected USB disconnect")
                    operation.side_effect = disconnect_once
                    with patch("esptool.cmds.time.sleep"), patch("esptool.cmds.read_flash", return_value=app) as read:
                        with self.assertRaisesRegex(SerialException, "injected USB disconnect"):
                            device.write_verified(address, app)
                    esp.flash_defl_begin.assert_called_once()
                    operation.assert_called_once()
                    esp.connect.assert_not_called()
                    esp._port.open.assert_not_called()
                    esp.run_stub.assert_called_once()
                    esp.flash_defl_finish.assert_not_called()
                    esp.flash_md5sum.assert_not_called()
                    read.assert_not_called()

    def test_real_esptool_write_failure_leaves_journal_and_blocks_retry(self):
        root, snapshot, application, images, report = self.fixture()
        device, esp = self.usb_fixture(application.read_bytes())
        device.read = MagicMock(side_effect=lambda address, size: next(
            data for name, data in images.items() if transition.REGIONS[name] == (address, size)))
        esp.flash_defl_block.side_effect = SerialException("injected USB disconnect")
        with patch.object(transition, "ROOT", root), patch.object(transition, "analyze", return_value=report), patch("esptool.cmds.time.sleep"):
            plan = transition.make_plan(snapshot, application)
            with self.assertRaises(SerialException):
                transition.apply(device, plan, root, plan["mac"])
            self.assertEqual(json.loads((snapshot / "apply-started.json").read_text()), plan)
            self.assertFalse((snapshot / "apply-verified.json").exists())
            reads = device.read.call_count
            with self.assertRaisesRegex(ValueError, "Previous write attempt"):
                transition.apply(device, plan, root, plan["mac"])
            self.assertEqual(device.read.call_count, reads)
        esp.flash_defl_begin.assert_called_once()
        esp.flash_defl_block.assert_called_once()
        esp.connect.assert_not_called()

    def test_real_esptool_success_retains_verification_and_readback(self):
        app = production_app()
        default_attempts = ESPLoader.WRITE_FLASH_ATTEMPTS
        device, esp = self.usb_fixture(app)
        with patch("esptool.cmds.read_flash", return_value=app) as read:
            device.write_verified(0x10000, app)
        self.assertEqual(esp.WRITE_FLASH_ATTEMPTS, 1)
        self.assertEqual(ESPLoader.WRITE_FLASH_ATTEMPTS, default_attempts)
        esp.flash_defl_begin.assert_called_once()
        esp.flash_defl_finish.assert_called_once()
        self.assertEqual(esp.flash_md5sum.call_count, 2)
        read.assert_called_once_with(esp, 0x10000, len(app), no_progress=True)
        esp.connect.assert_not_called()

    def test_real_esptool_verification_failures_do_not_rewrite(self):
        app = production_app()
        md5 = hashlib.md5(app).hexdigest()
        for stage in ("write_md5", "verify_md5", "readback"):
            with self.subTest(stage=stage):
                device, esp = self.usb_fixture(app)
                if stage == "write_md5":
                    esp.flash_md5sum.return_value = "0" * 32
                elif stage == "verify_md5":
                    esp.flash_md5sum.side_effect = [md5, "0" * 32]
                with patch("esptool.cmds.read_flash", return_value=b"changed") as read:
                    with self.assertRaises(ValueError if stage == "readback" else FatalError):
                        device.write_verified(0x10000, app)
                esp.flash_defl_begin.assert_called_once()
                esp.connect.assert_not_called()
                esp._port.open.assert_not_called()
                if stage != "readback":
                    read.assert_not_called()

    def test_usb_connects_only_the_explicit_port_and_leaves_it_stopped(self):
        esp = MagicMock()
        esp.read_mac.return_value = bytes.fromhex("aabbccddeeff")
        esp.get_security_info.return_value = dict(flags=0, flash_crypt_cnt=0)
        esp.run_stub.return_value = esp
        with patch("esptool.get_default_connected_device", return_value=esp) as connect, patch("esptool.cmds.attach_flash") as attach:
            def flash_id():
                esp.run_stub.assert_called_once()
                attach.assert_called_once_with(esp)
                return 0x1740c8
            esp.flash_id.side_effect = flash_id
            device = transition.Device("/dev/cu.test", "aa:bb:cc:dd:ee:ff")
            connect.assert_called_once_with(["/dev/cu.test"], "/dev/cu.test", 7, 115200, chip="esp32s3")
            device.close()
        esp._port.close.assert_called_once()
        esp.hard_reset.assert_not_called()

    def test_unexpected_flash_size_still_closes_without_writes(self):
        esp = MagicMock()
        esp.read_mac.return_value = bytes.fromhex("aabbccddeeff")
        esp.get_security_info.return_value = dict(flags=0, flash_crypt_cnt=0)
        esp.run_stub.return_value = esp
        esp.flash_id.return_value = 0x1640c8
        with patch("esptool.get_default_connected_device", return_value=esp), patch("esptool.cmds.attach_flash"):
            with self.assertRaisesRegex(ValueError, "8 MiB"):
                transition.Device("/dev/cu.test", "aa:bb:cc:dd:ee:ff")
        esp._port.close.assert_called_once()
        esp.write_flash.assert_not_called()

    def test_durable_payload_crc_and_namespace_ambiguity_fail_closed(self):
        payload = bytearray(192); payload[:4] = b"OAC1"
        struct.pack_into("<I", payload, 188, zlib.crc32(payload[:188]))
        namespace = SimpleNamespace(state="Written", metadata=dict(namespace=0, type="uint8", crc=dict(original=1, computed=1)),
                                    key="openathan", data=dict(value=1))
        record = SimpleNamespace(state="Written", metadata=dict(namespace=1, type="blob", crc=dict(original=1, computed=1, data_original=1, data_computed=1)),
                                 key="settings", data=dict(size=192), children=[SimpleNamespace(raw=payload)])
        page = SimpleNamespace(is_empty=False, header=dict(status="Active", crc=dict(original=1, computed=1)), entries=[namespace, record])
        parser = SimpleNamespace(NVS_Partition=lambda *_: SimpleNamespace(pages=[page]))
        self.assertTrue(transition.records(b"", parser)["openathan"]["present"])
        payload[10] ^= 1
        with self.assertRaisesRegex(ValueError, "durable"):
            transition.records(b"", parser)
        payload[10] ^= 1; page.entries.append(namespace)
        with self.assertRaisesRegex(ValueError, "namespace"):
            transition.records(b"", parser)

    def test_selection_and_corrupt_metadata(self):
        self.assertEqual(transition.selection(metadata(2))["offset"], 0x210000)
        self.assertEqual(transition.selection(metadata(state=0xffffffff))["state"], "undefined")
        self.assertEqual(transition.selection(b"\xff"*0x2000)["state"], "erased")
        damaged = bytearray(metadata()); damaged[28] ^= 1
        for invalid in (bytes(damaged), metadata(0), metadata(state=3), metadata(state=99), b"short"):
            with self.assertRaises(ValueError): transition.selection(invalid)

    def test_preservation_tracks_payloads_and_absent_namespaces(self):
        before = dict(records={"openathan": dict(present=True, records={"settings": "hash"}),
                               "oa_upgrade": dict(present=False, records={})}, regions={"audio": "hash"})
        transition.preservation(before, before)
        for changed in (dict(records={}, regions=before["regions"]),
                        dict(records=before["records"], regions={"audio": "changed"})):
            with self.assertRaises(ValueError): transition.preservation(before, changed)

    def fixture(self):
        temporary = tempfile.TemporaryDirectory(); self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        snapshot = root / "snapshot"; snapshot.mkdir(mode=0o700)
        app = production_app()
        application = root / "production.bin"; application.write_bytes(app)
        images = {name: bytes(size) for name, (_, size) in transition.REGIONS.items()}
        report = dict(mac="aa:bb:cc:dd:ee:ff", selection=dict(slot=0, offset=0x10000, state="valid"),
                      regions={name: digest(data) for name, data in images.items()}, records={})
        (snapshot / "snapshot.json").write_bytes(json_bytes(report))
        release = root / "release"; release.mkdir()
        (release / "rollback-bootloaders.json").write_text(json.dumps(dict(sha256=[report["regions"]["bootloader"]])))
        (release / "firmware.json").write_text('{"version":"v0.2.0"}')
        return root, snapshot, application, images, report

    def test_stale_application_or_device_never_writes(self):
        root, snapshot, application, images, report = self.fixture()
        class Device:
            writes = 0
            def read(self, address, size):
                return next(data for name, data in images.items() if transition.REGIONS[name] == (address, size))
            def write_verified(self, *args): self.writes += 1
        device = Device()
        with patch.object(transition, "ROOT", root), patch.object(transition, "analyze", return_value=report):
            plan = transition.make_plan(snapshot, application)
            application.write_bytes(application.read_bytes()[:-1]+b"x")
            with self.assertRaises(ValueError): transition.apply(device, plan, root, plan["mac"])
            self.assertEqual(device.writes, 0)
            application.write_bytes(production_app())
            plan = transition.make_plan(snapshot, application)
            changed = {**report, "regions": {**report["regions"], "control": "changed"}}
            with patch.object(transition, "analyze", return_value=changed):
                with self.assertRaises(ValueError): transition.apply(device, plan, root, plan["mac"])
            self.assertEqual(device.writes, 0)
            self.assertFalse((snapshot / "apply-started.json").exists())

    def test_production_restoration_rejects_test_images_before_flash_io(self):
        root, snapshot, application, images, report = self.fixture()
        device = MagicMock()
        with patch.object(transition, "ROOT", root):
            production_plan = transition.make_plan(snapshot, application)
            self.assertEqual(production_plan["offset"], 0x10000)
            for marker in ISOLATED_STORAGE_MARKERS + QUALIFICATION_MARKERS:
                with self.subTest(marker=marker):
                    app = production_app(marker)
                    application.write_bytes(app)
                    with self.assertRaises(ValueError):
                        transition.make_plan(snapshot, application)
                    # A correctly hashed plan from an older helper must also be
                    # refused before scoped flash reads, writes or the write journal.
                    legacy_plan = {**production_plan, "application_sha256": digest(app),
                                   "application_bytes": len(app)}
                    with self.assertRaises(ValueError):
                        transition.apply(device, legacy_plan, root, report["mac"])
                    device.read.assert_not_called()
                    device.write_verified.assert_not_called()
                    self.assertFalse((snapshot / "apply-started.json").exists())

    def test_write_failure_is_journaled_and_not_retried(self):
        root, snapshot, application, images, report = self.fixture()
        class Device:
            writes = 0
            def read(self, address, size):
                return next(data for name, data in images.items() if transition.REGIONS[name] == (address, size))
            def write_verified(self, *args):
                self.writes += 1; raise OSError("injected write failure")
        device = Device()
        with patch.object(transition, "ROOT", root), patch.object(transition, "analyze", return_value=report):
            plan = transition.make_plan(snapshot, application)
            with self.assertRaises(OSError): transition.apply(device, plan, root, plan["mac"])
            with self.assertRaisesRegex(ValueError, "Previous write attempt"):
                transition.apply(device, plan, root, plan["mac"])
        self.assertEqual(device.writes, 1)
        self.assertTrue((snapshot / "apply-started.json").exists())


if __name__ == "__main__":
    unittest.main()

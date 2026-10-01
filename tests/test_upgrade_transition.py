"""USB planning and fresh-state guards; no physical device is accessed."""
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
from release_artifacts import digest, json_bytes
from release_fixtures import esp_image


def production_app():
    payload = bytearray(256)
    struct.pack_into("<I", payload, 0, 0xabcd5432)
    payload[16:22] = b"v0.2.0"
    return esp_image(bytes(payload))


def metadata(sequence=1, state=2):
    data = bytearray(b"\xff"*0x2000)
    struct.pack_into("<I", data, 0, sequence)
    struct.pack_into("<II", data, 24, state, zlib.crc32(struct.pack("<I", sequence), 0xffffffff))
    return bytes(data)


class TransitionTests(unittest.TestCase):
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

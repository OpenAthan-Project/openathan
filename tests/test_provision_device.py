import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("provision", Path(__file__).resolve().parents[1] / "tools/provision_device.py")
provision = importlib.util.module_from_spec(spec)
spec.loader.exec_module(provision)


def response(extension, kind, data):
    packet = bytearray(provision.HEADERS[extension]) + bytes((1, kind, len(data))) + data
    packet.append(sum(packet) & 255)
    return packet + b"\n"


class Port:
    def __init__(self, output=b""):
        self.output = bytearray(output)
        self.writes = []
        self.now = 0

    def write(self, data):
        self.writes.append(data)

    def flush(self):
        pass

    def read(self, size):
        self.now += .01
        return bytes((self.output.pop(0),)) if self.output else b""


class ProvisioningConsoleTests(unittest.TestCase):
    def test_golden_improv_request(self):
        # Independent protocol fixture: GET_DEVICE_INFO, empty RPC body.
        self.assertEqual(provision.encode(False, 3), b"IMPROV\x01\x03\x02\x03\x00\xe6\n")

    def test_fragmentation_and_corruption(self):
        decoder = provision.Decoder()
        packet = response(True, 4, b"\x02\x06\x05saved")
        values = [decoder.feed(byte, 1) for byte in b"serial noise" + packet]
        self.assertIn((True, 4, b"\x02\x06\x05saved"), values)
        broken = bytearray(packet)
        broken[-2] ^= 1
        self.assertFalse(any(decoder.feed(byte, 2) for byte in broken))

    def test_acknowledgment_and_error(self):
        port = Port(response(True, 2, b"\0") + response(True, 4, b"\x02\x06\x05saved"))
        console = provision.Console(port, clock=lambda: port.now)
        self.assertEqual(console.command(True, 2, ("a longer password",)), ["saved"])
        self.assertEqual(len(port.writes), 1)
        port = Port(response(False, 2, b"\x03"))
        with self.assertRaisesRegex(RuntimeError, "previous credentials retained"):
            provision.Console(port, clock=lambda: port.now).command(False, 1)

    def test_lost_ack_is_never_retried(self):
        port = Port()
        with self.assertRaises(TimeoutError):
            provision.Console(port, timeout=.1, clock=lambda: port.now).command(True, 2, ("a longer password",))
        self.assertEqual(len(port.writes), 1)

    def test_status_storage_fault_and_urls(self):
        values = ["1", "4", "ready", "active", "3", "openathan-test.local", "fault",
                  "http://openathan-test.local/", "http://192.0.2.10/"]
        fields = b"".join(bytes((len(value),)) + value.encode() for value in values)
        port = Port(response(True, 4, bytes((1, len(fields))) + fields))
        status = provision.Console(port, clock=lambda: port.now).status()
        self.assertEqual(status["credential_storage"], "fault")
        self.assertEqual(status["urls"], values[7:])
        self.assertEqual(status["password_revision"], "3")

    def test_invalid_lengths(self):
        with self.assertRaises(ValueError):
            provision.encode(True, 2, ("a" * 254,))
        with self.assertRaises(ValueError):
            provision.decode_fields(b"\x01\x02\x04x")

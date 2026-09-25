#!/usr/bin/env python3
"""USB provisioning console; sends commands only, never flashes or erases a device."""
import argparse
import getpass
import json
import time

HEADERS = (b"IMPROV", b"OATHAN")


def encode(extension, command, fields=()):
    data = bytearray((command, 0))
    for field in fields:
        value = field.encode("utf-8")
        if b"\0" in value or len(value) > 251:
            raise ValueError("Invalid field length or content")
        data.append(len(value))
        data.extend(value)
    if len(data) > 255:
        raise ValueError("Command exceeds the USB frame limit")
    data[1] = len(data) - 2
    packet = bytearray(HEADERS[bool(extension)]) + bytes((1, 3, len(data))) + data
    packet.append(sum(packet) & 255)
    return bytes(packet) + b"\n"


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.last = 0

    def feed(self, byte, now):
        if now < self.last or now - self.last > 1:
            self.buffer.clear()
        self.last = now
        self.buffer.append(byte)
        if len(self.buffer) <= 6:
            while self.buffer and not any(header.startswith(self.buffer) for header in HEADERS):
                del self.buffer[0]
            return None
        if self.buffer[6] != 1:
            self.buffer.clear()
            return None
        if len(self.buffer) < 9 or len(self.buffer) < self.buffer[8] + 10:
            return None
        frame = bytes(self.buffer)
        self.buffer.clear()
        if sum(frame[:-1]) & 255 != frame[-1]:
            return None
        return frame[:6] == HEADERS[1], frame[7], frame[9:-1]


def decode_fields(data):
    if len(data) < 2 or data[1] != len(data) - 2:
        raise ValueError("Malformed device response")
    fields = []
    offset = 2
    while offset < len(data):
        size = data[offset]
        offset += 1
        if offset + size > len(data):
            raise ValueError("Truncated device response")
        fields.append(data[offset:offset + size].decode("utf-8"))
        offset += size
    return data[0], fields


class Console:
    def __init__(self, port, timeout=40, clock=time.monotonic):
        self.port, self.timeout, self.clock = port, timeout, clock
        self.decoder = Decoder()

    def command(self, extension, command, fields=(), scan=False):
        # No retries: a lost acknowledgment does not imply a failed write.
        self.port.write(encode(extension, command, fields))
        self.port.flush()
        deadline = self.clock() + self.timeout
        results = []
        while self.clock() < deadline:
            raw = self.port.read(1)
            if not raw:
                continue
            result = self.decoder.feed(raw[0], self.clock())
            if not result or result[0] != extension:
                continue
            _, kind, data = result
            if kind == 2 and data and data[0]:
                errors = {1: "Invalid command or credentials", 2: "Unsupported command",
                          3: "Wi-Fi connection failed; previous credentials retained",
                          255: "Device busy or storage unavailable; inspect status before retrying"}
                raise RuntimeError(errors.get(data[0], "Device rejected the command"))
            if kind != 4:
                continue
            response_command, response_fields = decode_fields(data)
            if response_command != command:
                continue
            if not scan:
                return response_fields
            if not response_fields:
                return results
            results.append(response_fields)
        raise TimeoutError("No acknowledgment. The outcome is uncertain; the command was not repeated")

    def status(self):
        fields = self.command(True, 1)
        if len(fields) < 7 or fields[0] != "1":
            raise RuntimeError("Unsupported OpenAthan provisioning protocol")
        return dict(protocol=fields[0], wifi_state=fields[1], password=fields[2],
                    setup=fields[3], password_revision=fields[4], hostname=fields[5],
                    credential_storage=fields[6], urls=fields[7:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Explicit serial port; close other serial clients first")
    parser.add_argument("--timeout", type=float, default=40)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("status")
    commands.add_parser("scan")
    wifi = commands.add_parser("wifi")
    wifi.add_argument("--ssid", required=True, help="Network name, including hidden networks")
    commands.add_parser("password")
    args = parser.parse_args()
    import serial
    # Set control lines before opening; do not intentionally reset the board.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=3)
    port.dtr = port.rts = False
    port.port = args.port
    try:
        port.open()
        console = Console(port, args.timeout)
        if args.command == "status":
            result = console.status()
        elif args.command == "scan":
            result = console.command(False, 4, scan=True)
        elif args.command == "wifi":
            password = getpass.getpass("Wi-Fi password: ")
            if not 1 <= len(args.ssid.encode()) <= 32 or not 8 <= len(password.encode()) <= 64:
                raise ValueError("Use an SSID up to 32 bytes and a WPA2/WPA3 personal-network password")
            result = console.command(False, 1, (args.ssid, password))
        else:
            password = getpass.getpass("Choose device password (12–128 printable ASCII characters): ")
            if not 12 <= len(password) <= 128 or any(not 32 <= ord(c) <= 126 for c in password):
                raise ValueError("Password must be 12–128 printable ASCII characters")
            if password != getpass.getpass("Repeat device password: "):
                raise ValueError("Passwords do not match")
            result = console.command(True, 2, (password,))
        print(json.dumps(result, indent=2))
    except TimeoutError as error:
        print(str(error))
        try:
            print(json.dumps(console.status(), indent=2))
        except (RuntimeError, TimeoutError, ValueError, serial.SerialException):
            print("Status unavailable. Reconnect and run status before retrying.")
        raise SystemExit(1) from None
    except (RuntimeError, ValueError, serial.SerialException) as error:
        parser.exit(1, str(error) + "\n")
    finally:
        port.close()


if __name__ == "__main__":
    main()

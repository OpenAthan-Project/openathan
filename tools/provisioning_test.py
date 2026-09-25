#!/usr/bin/env python3
"""Inspect or clear only isolated provisioning-test storage over USB. Never flashes."""
import argparse
import json
from provision_device import Console

TEST_NAMESPACES = ("oa_test", "oa_setup_test", "oa_network_test")


def inspect(console, expected_hostname):
    fields = console.command(True, 0x70)
    if (len(fields) != 3 or fields[0] != "test-v1" or fields[1] != expected_hostname or
            not expected_hostname.startswith("openathan-test-") or not expected_hostname.endswith(".local") or
            fields[2] not in ("ready", "restart_required")):
        raise ValueError("Device identity or test firmware mismatch; nothing cleared")
    return dict(protocol=fields[0], hostname=fields[1], state=fields[2])


def clear(console, expected_hostname, namespaces):
    # Validate before ANY device command. The firmware repeats this allowlist.
    if tuple(namespaces) != TEST_NAMESPACES:
        raise ValueError("Only the complete ordered test namespace set may be cleared")
    state = inspect(console, expected_hostname)
    if state["state"] != "ready":
        raise ValueError("Restart the device and inspect it before another maintenance command")
    # One write; no automatic retry after a lost acknowledgment.
    fields = console.command(True, 0x71, (expected_hostname, *TEST_NAMESPACES))
    if fields != ["cleared", "restart_required"]:
        raise RuntimeError("Cleanup outcome uncertain; keep the device stopped and inspect after restart")
    return dict(cleared=list(TEST_NAMESPACES), state="restart_required")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--expect-hostname", required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inspect")
    reset = commands.add_parser("clear")
    reset.add_argument("--namespaces", nargs=3, choices=TEST_NAMESPACES, required=True,
                       metavar="TEST_NAMESPACE")
    args = parser.parse_args()
    if args.command == "clear" and tuple(args.namespaces) != TEST_NAMESPACES:
        parser.error("Provide oa_test oa_setup_test oa_network_test in that order")
    import serial
    port = serial.Serial(port=None, baudrate=115200, timeout=.1, write_timeout=3)
    port.dtr = port.rts = False
    port.port = args.port
    try:
        port.open()
        console = Console(port)
        result = (inspect(console, args.expect_hostname) if args.command == "inspect" else
                  clear(console, args.expect_hostname, args.namespaces))
        print(json.dumps(result, indent=2))
    except (ValueError, RuntimeError, TimeoutError, serial.SerialException) as error:
        parser.exit(1, str(error) + ". No write was retried.\n")
    finally:
        port.close()


if __name__ == "__main__":
    main()

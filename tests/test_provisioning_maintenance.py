from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import provisioning_test as maintenance


class Console:
    def __init__(self, identity=None, lose=False):
        self.identity = identity or ["test-v1", "openathan-test-aabbcc.local", "ready"]
        self.calls = []
        self.lose = lose

    def command(self, extension, command, fields=()):
        self.calls.append((extension, command, fields))
        if command == 0x70:
            return self.identity
        if self.lose:
            raise TimeoutError("lost acknowledgment")
        return ["cleared", "restart_required"]


class MaintenanceTests(unittest.TestCase):
    def test_clear_handshake_and_exact_allowlist(self):
        console = Console()
        result = maintenance.clear(console, console.identity[1], maintenance.TEST_NAMESPACES)
        self.assertEqual(result["state"], "restart_required")
        self.assertEqual(console.calls[1], (True, 0x71, (console.identity[1], *maintenance.TEST_NAMESPACES)))

    def test_reject_other_and_partial_namespaces_before_io(self):
        for names in [("openathan",), ("oa_test",), ("oa_test", "oa_setup_test", "oa_network"),
                      ("oa_test", "oa_test", "oa_network_test"), ("oa_validation",)]:
            console = Console()
            with self.assertRaises(ValueError):
                maintenance.clear(console, console.identity[1], names)
            self.assertEqual(console.calls, [])

    def test_reject_identity_and_restart_required(self):
        for identity in [["production", "openathan-aabbcc.local", "ready"],
                         ["test-v1", "openathan-test-other.local", "ready"],
                         ["test-v1", "openathan-test-aabbcc.local", "restart_required"]]:
            console = Console(identity)
            with self.assertRaises(ValueError):
                maintenance.clear(console, "openathan-test-aabbcc.local", maintenance.TEST_NAMESPACES)
            self.assertEqual(len(console.calls), 1)

    def test_never_retry_uncertain_clear(self):
        console = Console(lose=True)
        with self.assertRaises(TimeoutError):
            maintenance.clear(console, console.identity[1], maintenance.TEST_NAMESPACES)
        self.assertEqual(len(console.calls), 2)

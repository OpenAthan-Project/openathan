import asyncio
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class SettingsConsoleTests(unittest.TestCase):
    def setUp(self):
        import device_console
        self.console = device_console
        self.document = {"schema": 1, "revision": 4, "settings": {"latitude": 43.6532, "longitude": -79.3832,
            "method": "north_america", "asr_method": "standard", "high_latitude": "middle_of_night",
            "offsets": dict.fromkeys(("fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"), 0),
            "enabled": dict.fromkeys(("fajr", "dhuhr", "asr", "maghrib", "isha"), True),
            "volume": 35, "timezone": "America/Toronto"}}

    def test_timezone_resolution_and_input_failures(self):
        for invalid in ([], None, {"schema":1,"revision":1,"settings":None}):
            with self.assertRaises(ValueError): self.console.settings_request(invalid)
        for name, offset in (("America/Toronto", 18000), ("Asia/Kolkata", -19800),
                             ("Australia/Sydney", -36000), ("UTC", 0)):
            self.document["settings"]["timezone"] = name
            request, payload = self.console.settings_request(self.document)
            self.assertEqual(request["settings"]["timezone_rules"]["standard_offset"], offset)
            self.assertEqual(request, json.loads(payload))
            self.assertEqual(request["expected_revision"], 4)
        self.document["settings"]["timezone"] = "Invented/Nowhere"
        with self.assertRaises(ValueError): self.console.settings_request(self.document)
        self.document["settings"]["timezone"] = "UTC"
        self.document["revision"] = True
        with self.assertRaises(ValueError): self.console.settings_request(self.document)
        self.document["revision"] = 4
        self.document["settings"]["latitude"] = float("nan")
        with self.assertRaises(ValueError): self.console.settings_request(self.document)

    def test_private_export(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            self.console.export_settings(path, self.document)
            self.assertEqual(json.loads(path.read_text()), self.document)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            link = Path(directory) / "link.json"
            link.symlink_to(path)
            with self.assertRaises(OSError): self.console.export_settings(link, {})
        with self.assertRaises(ValueError): self.console.export_settings(ROOT / "settings.json", {})

    def test_uncertain_disconnect_reads_back_without_resending(self):
        request, _ = self.console.settings_request(self.document)
        snapshot = {"schema":1, "revision":5, "settings":request["settings"], "application":"applied"}
        calls = []
        clock = [0.0]
        class Client:
            def __init__(self, *args, **kwargs): pass
            async def connect(self, **kwargs): pass
            async def disconnect(self):
                if calls == ["set_settings"]:
                    raise ConnectionResetError("power cut during disconnect handshake")
            async def device_info(self): return SimpleNamespace(name="openathan-feasibility", esphome_version="2026.9.0")
            async def list_entities_services(self): return [], [SimpleNamespace(name=n) for n in ("get_settings","set_settings")]
            def subscribe_logs(self, *args, **kwargs): pass
            def subscribe_states(self, *args, **kwargs): pass
            async def execute_service(self, service, data, **kwargs):
                calls.append(service.name)
                if service.name == "set_settings": raise ConnectionError("disconnected after commit")
                return SimpleNamespace(success=True, response_data=json.dumps(snapshot).encode())
        async def sleep(_): clock[0] += 1
        async def wait(awaitable, **kwargs):
            awaitable.close()
            clock[0] = 31
            raise asyncio.TimeoutError()
        with tempfile.TemporaryDirectory() as directory:
            settings = Path(directory) / "settings.json"; settings.write_text(json.dumps(self.document))
            secrets = Path(directory) / "secrets.yaml"; secrets.write_text("diagnostic_api_key: test-key")
            args = SimpleNamespace(host="unused", seconds=30, press=None, get_settings=None, set_settings=settings, secrets=secrets)
            with patch.object(self.console,"APIClient",Client), patch.object(self.console,"emit"), \
                 patch.object(self.console.time,"monotonic",lambda:clock[0]), \
                 patch.object(self.console.asyncio,"sleep",sleep), patch.object(self.console.asyncio,"wait_for",wait):
                asyncio.run(self.console.run(args))
        self.assertEqual(calls,["set_settings","get_settings"])

    def test_device_rejection_is_not_success(self):
        class Client:
            async def execute_service(self, *args, **kwargs):
                return SimpleNamespace(success=False, response_data=b"", error_message="revision conflict")
        with self.assertRaisesRegex(ValueError,"revision conflict"):
            asyncio.run(self.console.settings_action(Client(),[SimpleNamespace(name="set_settings")],"set_settings",{}))

#!/usr/bin/env python3
"""Private local validation console; run in the pinned ESPHome environment.

Store output and the secrets YAML outside the repository. Commands address only
named buttons explicitly exposed by the developer image; no Home Assistant needed.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import time

from aioesphomeapi import APIClient, LogLevel
import yaml

ROOT = Path(__file__).resolve().parents[1]


def settings_request(document):
    """Resolve timezone locally; send one complete, revision-checked snapshot."""
    from esphome.components.time import validate_tz
    from esphome.config_validation import Invalid
    from aioesphomeapi.posix_tz import parse_posix_tz
    if (not isinstance(document, dict) or type(document.get("schema")) is not int or document["schema"] != 1 or
            type(document.get("revision")) is not int or not 1 <= document["revision"] <= 0xFFFFFFFF or
            not isinstance(document.get("settings"), dict)):
        raise ValueError("Expected a schema 1 settings export with a positive revision")
    settings = dict(document["settings"])
    try:
        parsed = parse_posix_tz(validate_tz(settings["timezone"]))
    except Invalid as error:
        raise ValueError(str(error)) from error
    def rule(value):
        return {"type": int(value.type), "time_seconds": value.time_seconds, "day": value.day,
                "month": value.month, "week": value.week, "day_of_week": value.day_of_week}
    settings["timezone_rules"] = {"standard_offset": parsed.std_offset_seconds,
        "daylight_offset": parsed.dst_offset_seconds if parsed.dst_start.type else 0,
        "start": rule(parsed.dst_start), "end": rule(parsed.dst_end)}
    request = {"schema": 1, "expected_revision": document["revision"], "settings": settings}
    payload = json.dumps(request, allow_nan=False, separators=(",", ":"))
    if len(payload.encode()) > 4096:
        raise ValueError("Settings document exceeds 4096 bytes")
    return request, payload


def export_settings(path, document):
    if path.resolve().is_relative_to(ROOT):
        raise ValueError("Keep private settings exports outside the repository")
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "w") as handle:
        os.fchmod(handle.fileno(), 0o600)
        json.dump(document, handle, indent=2, allow_nan=False)
        handle.write("\n")


async def settings_action(client, services, name, data):
    service = next((service for service in services if service.name == name), None)
    if service is None:
        raise ValueError("Settings action unavailable: " + name)
    response = await client.execute_service(service, data, return_response=True)
    if response is None:
        raise RuntimeError("Device did not acknowledge the settings action")
    snapshot = json.loads(response.response_data) if response.response_data else None
    if not response.success:
        raise ValueError(response.error_message or "Device rejected settings")
    if not isinstance(snapshot, dict) or snapshot.get("schema") != 1:
        raise RuntimeError("Invalid settings response")
    return snapshot


def emit(kind, value):
    print(json.dumps({"time": datetime.now(timezone.utc).isoformat(), "kind": kind, "value": value}), flush=True)


async def run(args):
    secrets = yaml.safe_load(args.secrets.read_text())
    deadline = time.monotonic() + args.seconds
    pressed = False
    settings_sent = False
    settings_confirmed = False
    settings_exported = False
    request, payload = settings_request(json.loads(args.set_settings.read_text())) if args.set_settings else (None, None)
    while time.monotonic() < deadline:
        client = APIClient(args.host, 6053, noise_psk=secrets["diagnostic_api_key"],
                           expected_name="openathan-feasibility")
        disconnected = asyncio.Event()
        async def on_stop(expected):
            disconnected.set()
        try:
            await client.connect(on_stop=on_stop, login=True)
            info = await client.device_info()
            emit("connected", {"name": info.name, "version": info.esphome_version})
            entities, services = await client.list_entities_services()
            buttons = {e.name: e.key for e in entities if e.__class__.__name__ == "ButtonInfo"}
            emit("buttons", list(buttons))
            client.subscribe_logs(lambda m: emit("log", m.message.decode(errors="replace")),
                                  log_level=LogLevel.LOG_LEVEL_INFO, dump_config=False)
            client.subscribe_states(lambda s: emit("state", str(s)))
            if args.get_settings or args.set_settings:
                if args.set_settings and not settings_sent:
                    # Mark before sending: a disconnect cannot safely authorize another write.
                    settings_sent = True
                    snapshot = await settings_action(client, services, "set_settings", {"payload": payload})
                    settings_confirmed = True
                    emit("settings_saved", snapshot)
                else:
                    snapshot = await settings_action(client, services, "get_settings", {})
                    emit("settings_readback", snapshot)
                    if request:
                        settings_confirmed = (snapshot.get("settings") == request["settings"] and
                            snapshot.get("revision") in (request["expected_revision"], request["expected_revision"] + 1) and
                            snapshot.get("application") != "storage_fault")
                        emit("settings_reconciled", settings_confirmed)
                if args.get_settings:
                    export_settings(args.get_settings, snapshot)
                    settings_exported = True
                    emit("settings_exported", str(args.get_settings))
                if args.set_settings and not settings_confirmed:
                    raise ValueError("Settings outcome differs from the requested snapshot; inspect readback before another edit")
            if args.press and not pressed:
                if args.press not in buttons:
                    raise ValueError("Button unavailable: " + args.press)
                # Do not retry an uncertain command after disconnect.
                pressed = True
                client.button_command(buttons[args.press])
                emit("pressed", args.press)
            try:
                await asyncio.wait_for(disconnected.wait(), timeout=max(0.1, deadline-time.monotonic()))
            except asyncio.TimeoutError:
                pass
        except ValueError:
            raise
        except Exception as error:
            emit("connection_error", type(error).__name__)
        finally:
            try:
                await client.disconnect()
            except Exception as error:
                # A hard power cut can fail the disconnect handshake too. Keep
                # reconnecting for readback; never turn cleanup into a resend.
                emit("disconnect_error", type(error).__name__)
        if time.monotonic() < deadline:
            await asyncio.sleep(min(3, deadline-time.monotonic()))
    if args.press and not pressed:
        raise RuntimeError("No command sent: device connection unavailable")
    if args.set_settings and not settings_confirmed:
        raise RuntimeError("Settings save unconfirmed; read settings before retrying")
    if args.get_settings and not settings_exported:
        raise RuntimeError("Settings export unavailable: device connection unavailable")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--secrets", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--press")
    parser.add_argument("--get-settings", type=Path, metavar="JSON", help="Export the saved settings outside the repository")
    parser.add_argument("--set-settings", type=Path, metavar="JSON", help="Apply an edited settings export once")
    try:
        asyncio.run(run(parser.parse_args()))
    except (ValueError, OSError, KeyError, RuntimeError) as error:
        parser.error(str(error))

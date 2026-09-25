#!/usr/bin/env python3
"""Private local validation console; run in the pinned ESPHome environment.

Store output and the secrets YAML outside the repository. Commands address only
named buttons explicitly exposed by the developer image; no Home Assistant needed.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import time

from aioesphomeapi import APIClient, LogLevel
import yaml


def emit(kind, value):
    print(json.dumps({"time": datetime.now(timezone.utc).isoformat(), "kind": kind, "value": value}), flush=True)


async def run(args):
    secrets = yaml.safe_load(args.secrets.read_text())
    deadline = time.monotonic() + args.seconds
    pressed = False
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
            entities, _ = await client.list_entities_services()
            buttons = {e.name: e.key for e in entities if e.__class__.__name__ == "ButtonInfo"}
            emit("buttons", list(buttons))
            client.subscribe_logs(lambda m: emit("log", m.message.decode(errors="replace")),
                                  log_level=LogLevel.LOG_LEVEL_INFO, dump_config=False)
            client.subscribe_states(lambda s: emit("state", str(s)))
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
            await client.disconnect()
        if time.monotonic() < deadline:
            await asyncio.sleep(min(3, deadline-time.monotonic()))
    if args.press and not pressed:
        raise RuntimeError("No command sent: device connection unavailable")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--secrets", type=Path, required=True)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--press")
    asyncio.run(run(parser.parse_args()))

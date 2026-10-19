#!/usr/bin/env python3
"""Private signed dua fixtures. This tool never accesses or installs on a device."""

import argparse
from http.server import BaseHTTPRequestHandler
import json
from pathlib import Path
import re
import ssl
import time
from cryptography.hazmat.primitives import serialization

import audio_image
import dua_media
import release
from release_artifacts import (
    approved_tracks,
    digest,
    json_bytes,
    read_file,
    read_json,
    require,
)
from upgrade_qualification import (
    initialize as initialize_trust,
    private_directory,
    write_private,
    tls_server,
    lan_address,
)

VERSION = "v0.0.1"
MARKER = "OPENATHAN_DUA_QUALIFICATION_V1"
FAULTS = ("none", "signature", "hash", "truncate", "descriptor-disconnect", "stall")


def initialize(directory, address, port=8443):
    result = initialize_trust(directory, address, port)
    write_private(
        directory / "dua-control.json", json_bytes(dict(fault="none", delay=0))
    )
    return result


def stage(directory, commit, normal, fajr, dua):
    directory = private_directory(directory)
    require(
        re.fullmatch(r"[0-9a-f]{40}", commit), "Use an exact approval source commit"
    )
    payloads = tuple(audio_image.read_recording(path) for path in (normal, fajr, dua))
    approved_tracks(
        read_json(release.source_file(commit, release.MEDIA_REGISTRY)),
        release.source_file(commit, release.LICENSE_PATH),
        payloads,
    )
    release.validate_dua_recording(dua)
    public = read_file(directory / "public-key.pem", 4096)
    production = serialization.load_pem_public_key(
        release.source_file(commit, "release/upgrade-public-key.pem")
    )
    require(
        serialization.load_pem_public_key(public).public_numbers()
        != production.public_numbers(),
        "Use private qualification trust",
    )
    envelope = dua_media.sign(
        VERSION, payloads[2], read_file(directory / "signing-key.pem", 4096), public
    )
    baseline, _ = audio_image.build_image(*payloads[:2])
    extended, _ = audio_image.build_image(
        *payloads[:2], dua=payloads[2], dua_envelope=envelope
    )
    require(
        baseline[: dua_media.BANK_START] == extended[: dua_media.BANK_START],
        "Athan data changed",
    )
    destination = directory / VERSION
    destination.mkdir(mode=0o700)
    for name, data in (
        ("dua.mp3", payloads[2]),
        ("dua.json", envelope),
        ("baseline-audio.bin", baseline),
        ("fresh-install-private-audio.bin", extended),
    ):
        write_private(destination / name, data)
    record = dict(
        schema=1,
        version=VERSION,
        approval_commit=commit,
        recordings=[
            dict(role=role, bytes=len(data), sha256=digest(data))
            for role, data in zip(("normal", "fajr", "dua"), payloads)
        ],
        baseline_sha256=digest(baseline),
        extended_sha256=digest(extended),
    )
    write_private(destination / "stage.json", json_bytes(record))
    return record


def offer(directory, fault="none", delay=0):
    directory = private_directory(directory)
    require(fault in FAULTS and 0 <= delay <= 10, "Invalid private dua fault or delay")
    require(
        (directory / VERSION / "stage.json").is_file(), "Stage approved media first"
    )
    temporary = directory / "dua-control.tmp"
    write_private(temporary, json_bytes(dict(fault=fault, delay=delay)))
    temporary.replace(directory / "dua-control.json")


def handler(directory):
    class Feed(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            prefix = f"/releases/download/{VERSION}/"
            if self.path not in (prefix + "dua.json", prefix + "dua.mp3"):
                self.send_error(404)
                return
            control = read_json(read_file(directory / "dua-control.json", 4096))
            fault, delay = control["fault"], control["delay"]
            require(fault in FAULTS and 0 <= delay <= 10, "Invalid feed control")
            descriptor = self.path.endswith("dua.json")
            data = read_file(
                directory / VERSION / ("dua.json" if descriptor else "dua.mp3"),
                dua_media.MAX_ENVELOPE if descriptor else dua_media.MAX_BYTES,
            )
            if descriptor and fault == "signature":
                doc = json.loads(data)
                doc["signature"] = (
                    "00" if doc["signature"][:2] != "00" else "01"
                ) + doc["signature"][2:]
                data = json_bytes(doc)
            if not descriptor and fault == "hash":
                data = data[:-1] + bytes([data[-1] ^ 1])
            self.send_response(200)
            self.send_header(
                "Content-Type", "application/json" if descriptor else "audio/mpeg"
            )
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            try:
                if fault == "stall":
                    time.sleep(delay)
                cut = (descriptor and fault == "descriptor-disconnect") or (
                    not descriptor and fault == "truncate"
                )
                outgoing = data[: len(data) // 2] if cut else data
                for start in range(0, len(outgoing), 4096):
                    self.wfile.write(outgoing[start : start + 4096])
                    self.wfile.flush()
                    if delay and fault != "stall":
                        time.sleep(delay)
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass
            self.close_connection = True

    return Feed


def server(directory, address=None, port=None):
    directory = private_directory(directory)
    config = read_json(read_file(directory / "feed.json", 4096))
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(directory / "server.pem", directory / "server-key.pem")
    return tls_server(
        (
            address or lan_address(config["address"]),
            config["port"] if port is None else port,
        ),
        directory,
        context,
        handler,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("--directory", type=Path, required=True)
    init.add_argument("--address", required=True)
    init.add_argument("--port", type=int, default=8443)
    media = commands.add_parser("stage")
    media.add_argument("--directory", type=Path, required=True)
    media.add_argument("--commit", required=True)
    for name in ("normal", "fajr", "dua"):
        media.add_argument("--" + name, type=Path, required=True)
    control = commands.add_parser("offer")
    control.add_argument("--directory", type=Path, required=True)
    control.add_argument("--fault", choices=FAULTS, default="none")
    control.add_argument("--delay", type=float, default=0)
    serve = commands.add_parser("serve")
    serve.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "init":
        print(json.dumps(initialize(args.directory, args.address, args.port), indent=2))
    elif args.command == "stage":
        print(
            json.dumps(
                stage(args.directory, args.commit, args.normal, args.fajr, args.dua),
                indent=2,
            )
        )
    elif args.command == "offer":
        offer(args.directory, args.fault, args.delay)
    else:
        with server(args.directory) as feed:
            print("Private dua feed ready; no device action is initiated", flush=True)
            feed.serve_forever()


if __name__ == "__main__":
    main()

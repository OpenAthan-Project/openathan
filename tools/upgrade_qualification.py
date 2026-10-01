#!/usr/bin/env python3
"""Private LAN HTTPS feed and observation tools for isolated OTA qualification."""
import argparse
from datetime import datetime, timedelta, timezone
import getpass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import ssl
import sys
import threading
import time
from urllib.request import HTTPDigestAuthHandler, HTTPPasswordMgrWithDefaultRealm, Request, build_opener

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID
from audio_image import ROOT
from release_artifacts import digest, json_bytes, read_file, read_json, require
from upgrade_artifacts import sign

MARKER = b"OPENATHAN_QUALIFICATION_V1"
VERSIONS = ("v0.0.1", "v0.0.2", "v0.0.3", "v0.0.4")
FAULTS = ("none", "signature", "corrupt", "truncate", "disconnect")


def lan_address(value):
    address = ipaddress.IPv4Address(value)
    networks = ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")
    require(any(address in ipaddress.IPv4Network(network) for network in networks), "Use an RFC1918 LAN IPv4 address")
    return str(address)


def private_directory(path, fresh=False):
    path = path.absolute()
    require(not path.resolve().is_relative_to(ROOT.resolve()), "Keep qualification files outside Git")
    require(not path.is_symlink(), "Qualification directory must not be a symlink")
    if fresh:
        path.mkdir(mode=0o700)
    require(path.is_dir() and path.stat().st_mode & 0o077 == 0, "Qualification directory must have mode 700")
    return path


def write_private(path, data):
    with path.open("xb") as handle:
        handle.write(data)
    path.chmod(0o600)


def initialize(directory, address, port=8443):
    address = lan_address(address)
    require(1024 <= port <= 65535, "Invalid HTTPS port")
    directory = private_directory(directory, fresh=True)
    now = datetime.now(timezone.utc)
    def key():
        return ec.generate_private_key(ec.SECP256R1())
    ca_key, server_key, signing_key = key(), key(), key()
    ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "OpenAthan private qualification CA")])
    ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name)
          .public_key(ca_key.public_key()).serial_number(x509.random_serial_number())
          .not_valid_before(now - timedelta(minutes=5)).not_valid_after(now + timedelta(days=30))
          .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
          .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
          .add_extension(x509.SubjectKeyIdentifier.from_public_key(ca_key.public_key()), critical=False)
          .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
          .sign(ca_key, hashes.SHA256()))
    server = (x509.CertificateBuilder()
              .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, address)]))
              .issuer_name(ca_name).public_key(server_key.public_key()).serial_number(x509.random_serial_number())
              .not_valid_before(now - timedelta(minutes=5)).not_valid_after(now + timedelta(days=7))
              .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
              .add_extension(x509.KeyUsage(True, False, False, False, False, False, False, False, False), critical=True)
              .add_extension(x509.SubjectKeyIdentifier.from_public_key(server_key.public_key()), critical=False)
              .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
              .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.IPv4Address(address)),
                                                         x509.DNSName(address)]), critical=False)
              .add_extension(x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
              .sign(ca_key, hashes.SHA256()))
    for name, value in (("ca-key.pem", ca_key), ("server-key.pem", server_key), ("signing-key.pem", signing_key)):
        write_private(directory / name, value.private_bytes(serialization.Encoding.PEM,
                      serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    for name, value in (("ca.pem", ca), ("server.pem", server)):
        write_private(directory / name, value.public_bytes(serialization.Encoding.PEM))
    write_private(directory / "public-key.pem", signing_key.public_key().public_bytes(
                  serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
    write_private(directory / "feed.json", json_bytes(dict(address=address, port=port)))
    write_private(directory / "control.json", json_bytes(dict(version=None, fault="none", chunk_delay=0, header_delay=0)))
    return dict(origin=f"https://{address}:{port}", ca_file=str(directory / "ca.pem"),
                public_key_file=str(directory / "public-key.pem"))


def stage(directory, app_path, version, commit):
    directory = private_directory(directory)
    require(version in VERSIONS, "Unknown qualification version")
    app = read_file(app_path, 1572864)
    require(MARKER in app, "Expected a qualification application; never serve production firmware")
    envelope = sign(version, commit, app, read_file(directory / "signing-key.pem", 4096),
                    read_file(directory / "public-key.pem", 4096))
    destination = directory / version
    destination.mkdir(mode=0o700)  # Immutable version directories; never replace a candidate.
    write_private(destination / "firmware.ota.bin", app)
    write_private(destination / "upgrade.json", envelope)
    return dict(version=version, bytes=len(app), sha256=digest(app))


def offer(directory, version, fault="none", chunk_delay=0, header_delay=0):
    directory = private_directory(directory)
    require(version in VERSIONS and fault in FAULTS, "Invalid feed control")
    require(0 <= chunk_delay <= 10 and 0 <= header_delay <= 10, "Invalid response delay")
    require((directory / version / "upgrade.json").is_file(), "Stage the candidate first")
    data = json_bytes(dict(version=version, fault=fault, chunk_delay=chunk_delay, header_delay=header_delay))
    temporary = directory / f"control-{os.getpid()}-{threading.get_ident()}.tmp"
    write_private(temporary, data)
    os.replace(temporary, directory / "control.json")


def handler(directory):
    class Feed(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # No device credentials or request headers in logs.

        def do_GET(self):
            try:
                control = read_json(read_file(directory / "control.json", 4096))
                version = control["version"]
                if self.path == "/releases/latest/download/upgrade.json" and version in VERSIONS:
                    data = read_file(directory / version / "upgrade.json", 8192)
                    if control["fault"] == "signature":
                        envelope = read_json(data)
                        signature = envelope["signature"]
                        envelope["signature"] = ("0" if signature[0] != "0" else "1") + signature[1:]
                        data = json_bytes(envelope)
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(data)))
                    self.send_header("Cache-Control", "no-store")
                    self.end_headers()
                    self.wfile.write(data)
                    return
                requested = next((v for v in VERSIONS if self.path == f"/releases/download/{v}/firmware.ota.bin"), None)
                if requested is None:
                    self.send_error(404); return
                data = read_file(directory / requested / "firmware.ota.bin", 1572864)
                fault = control["fault"] if requested == version else "none"
                if fault == "corrupt":
                    changed = bytearray(data); changed[len(data)//2] ^= 1; data = bytes(changed)
                time.sleep(control["header_delay"])
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.end_headers()
                limit = len(data)//2 if fault in ("truncate", "disconnect") else len(data)
                for offset in range(0, limit, 4096):
                    self.wfile.write(data[offset:min(offset+4096, limit)])
                    self.wfile.flush()
                    time.sleep(control["chunk_delay"])
                self.close_connection = True
            except (OSError, ValueError, KeyError, TypeError):
                self.close_connection = True
    return Feed


def tls_server(address, directory, context):
    class TLSFeed(ThreadingHTTPServer):
        daemon_threads = True

        def get_request(self):
            connection, address = super().get_request()
            connection.settimeout(5)
            return connection, address

        def process_request_thread(self, connection, address):
            # Handshake in the bounded connection worker, never in accept(). An
            # idle TCP client must not block the feed's other HTTPS clients.
            try:
                connection = context.wrap_socket(connection, server_side=True)
            except (OSError, ssl.SSLError):
                connection.close()
                return
            super().process_request_thread(connection, address)

    return TLSFeed(address, handler(directory))


def server(directory):
    directory = private_directory(directory)
    config = read_json(read_file(directory / "feed.json", 4096))
    address = lan_address(config["address"])
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory / "server.pem", directory / "server-key.pem")
    return tls_server((address, config["port"]), directory, context)


def device_client(host, password):
    # Device-local Digest authentication, using the same protected API as its UI.
    import re
    require(re.fullmatch(r"(?:openathan-test-[a-zA-Z0-9-]+\.local|(?:\d{1,3}\.){3}\d{1,3})", host), "Use the test hostname or LAN IP")
    if not host.endswith(".local"):
        lan_address(host)
    origin = f"http://{host}"
    credentials = HTTPPasswordMgrWithDefaultRealm()
    credentials.add_password(None, origin, "admin", password)
    return origin, build_opener(HTTPDigestAuthHandler(credentials))


def observe(host, output, seconds, interval=2, command=None, phase=None):
    require(seconds > 0 and 1 <= interval <= 60, "Invalid observation duration")
    output = output.absolute()
    require(not output.resolve().is_relative_to(ROOT.resolve()), "Keep device evidence outside Git")
    origin, client = device_client(host, getpass.getpass("Test device password: "))
    with output.open("xb") as log:
        output.chmod(0o600)
        def exchange(request):
            with client.open(request, timeout=10) as response:
                data = read_json(response.read(65537))
            require(data.get("qualification", {}).get("identity") == MARKER.decode(), "Not qualification firmware")
            log.write((json.dumps(dict(utc=datetime.now(timezone.utc).isoformat(), firmware=data),
                                  separators=(",", ":")) + "\n").encode())
            log.flush()
            return data
        # Read and verify identity before any optional mutation. No POST retry.
        exchange(Request(origin + "/api/firmware"))
        if command:
            payload = dict(command=command)
            if command == "arm":
                require(phase in ("before_boot_selection", "after_boot_selection"), "Choose a hold phase")
                payload["phase"] = phase
            exchange(Request(origin + "/api/firmware/qualification", data=json_bytes(payload),
                             headers={"Content-Type": "application/json"}))
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            time.sleep(min(interval, max(0, deadline-time.monotonic())))
            exchange(Request(origin + "/api/firmware"))


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("--directory", type=Path, required=True)
    init.add_argument("--address", required=True)
    init.add_argument("--port", type=int, default=8443)
    staging = commands.add_parser("stage")
    staging.add_argument("--directory", type=Path, required=True)
    staging.add_argument("--application", type=Path, required=True)
    staging.add_argument("--version", choices=VERSIONS, required=True)
    staging.add_argument("--commit", required=True)
    offering = commands.add_parser("offer")
    offering.add_argument("--directory", type=Path, required=True)
    offering.add_argument("--version", choices=VERSIONS, required=True)
    offering.add_argument("--fault", choices=FAULTS, default="none")
    offering.add_argument("--chunk-delay", type=float, default=0)
    offering.add_argument("--header-delay", type=float, default=0)
    serving = commands.add_parser("serve")
    serving.add_argument("--directory", type=Path, required=True)
    observing = commands.add_parser("observe")
    observing.add_argument("--host", required=True)
    observing.add_argument("--output", type=Path, required=True)
    observing.add_argument("--seconds", type=int, default=60)
    observing.add_argument("--interval", type=int, default=2)
    observing.add_argument("--control", choices=("arm", "release"))
    observing.add_argument("--phase", choices=("before_boot_selection", "after_boot_selection"))
    args = parser.parse_args()
    try:
        if args.command == "init":
            print(json.dumps(initialize(args.directory, args.address, args.port), indent=2))
        elif args.command == "stage":
            print(json.dumps(stage(args.directory, args.application, args.version, args.commit), indent=2))
        elif args.command == "offer":
            offer(args.directory, args.version, args.fault, args.chunk_delay, args.header_delay)
        elif args.command == "serve":
            with server(args.directory) as feed:
                print("Private qualification HTTPS feed ready", flush=True)
                feed.serve_forever()
        else:
            observe(args.host, args.output, args.seconds, args.interval, args.control, args.phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}. Reconcile uncertain device actions before retrying.\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Private HTTPS fixtures for isolated Quran qualification; never a public feed."""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import ssl
import subprocess
import threading
import time
from urllib.parse import urlsplit

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID
from audio_image import ROOT
from release_artifacts import require
from upgrade_qualification import lan_address, private_directory, write_private

MARKER = "OPENATHAN_QURAN_QUALIFICATION_V1"
PREFIX = "/" + MARKER + "/"
CASES = ("normal", "metadata-status", "metadata-malformed", "metadata-oversize", "metadata-truncate", "metadata-stall",
         "metadata-redirect-http", "metadata-redirect-host", "metadata-redirect-loop",
         "audio-status", "audio-content-type", "audio-decode", "audio-truncate", "audio-stall",
         "audio-redirect-http", "audio-redirect-host", "audio-redirect-loop")
MEDIA_CASES = ("normal", "audio-truncate", "audio-stall")
MAX_MEDIA = 32 * 1024 * 1024
SOCKET_TIMEOUT_SECONDS = 5
BODY_CHUNK_BYTES = 4096


def initialize(directory, address, port=8443):
    address = lan_address(address)
    require(1024 <= port <= 65535, "Invalid HTTPS port")
    directory = private_directory(directory, fresh=True)
    now = datetime.now(timezone.utc)
    ca_key, server_key = (ec.generate_private_key(ec.SECP256R1()) for _ in range(2))
    ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "OpenAthan Quran private qualification CA")])
    ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name)
          .public_key(ca_key.public_key()).serial_number(x509.random_serial_number())
          .not_valid_before(now - timedelta(minutes=5)).not_valid_after(now + timedelta(days=7))
          .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
          .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
          .add_extension(x509.SubjectKeyIdentifier.from_public_key(ca_key.public_key()), critical=False)
          .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), critical=False)
          .sign(ca_key, hashes.SHA256()))
    for mode, name, signer in (("trusted", address, ca_key), ("wrong-host", "192.0.2.1", ca_key),
                               ("untrusted", address, server_key)):
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
        certificate = (x509.CertificateBuilder().subject_name(subject)
                       .issuer_name(subject if mode == "untrusted" else ca_name)
                       .public_key(server_key.public_key()).serial_number(x509.random_serial_number())
                       .not_valid_before(now - timedelta(minutes=5)).not_valid_after(now + timedelta(days=2))
                       .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
                       .add_extension(x509.KeyUsage(True, False, False, False, False, False, False, False, False), critical=True)
                       .add_extension(x509.SubjectKeyIdentifier.from_public_key(server_key.public_key()), critical=False)
                       .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(signer.public_key()), critical=False)
                       .add_extension(x509.SubjectAlternativeName([
                           x509.IPAddress(ipaddress.IPv4Address(name)), x509.DNSName(name)]), critical=False)
                       .add_extension(x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
                       .sign(signer, hashes.SHA256()))
        write_private(directory / f"server-{mode}.pem", certificate.public_bytes(serialization.Encoding.PEM))
    for name, key in (("ca-key.pem", ca_key), ("server-key.pem", server_key)):
        write_private(directory / name, key.private_bytes(serialization.Encoding.PEM,
                      serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    write_private(directory / "ca.pem", ca.public_bytes(serialization.Encoding.PEM))
    write_private(directory / "feed.json", json.dumps(dict(address=address, port=port)).encode())
    write_private(directory / "control.json", json.dumps(dict(case="metadata-malformed", stall_seconds=40)).encode())
    return dict(origin=f"https://{address}:{port}", ca_file=str(directory / "ca.pem"))


def validate_media(path):
    path = path.absolute()
    require(not path.resolve().is_relative_to(ROOT.resolve()) and not path.is_symlink(), "Keep real test media outside Git")
    require(path.suffix.lower() == ".mp3" and 0 < path.stat().st_size <= MAX_MEDIA, "Use a bounded real MP3 recording")
    # A suffix or random format fixture is not proof of playable media. No skip
    # switch is exposed: CI's non-decodable audio must never reach a device.
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-xerror", "-f", "mp3", "-i", str(path),
                    "-map", "0:a:0", "-f", "null", "-"], check=True, timeout=120, capture_output=True)
    return dict(media=str(path), media_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def select(directory, case, media=None, stall_seconds=40):
    directory = private_directory(directory)
    require(case in CASES and 0 <= stall_seconds <= 45, "Invalid fixture case or stall duration")
    control = dict(case=case, stall_seconds=stall_seconds)
    if case in MEDIA_CASES:
        require(media is not None, "This case requires an approved real MP3; CI audio is forbidden")
        control.update(validate_media(media))
    temporary = directory / f"control-{os.getpid()}-{threading.get_ident()}.tmp"
    write_private(temporary, json.dumps(control).encode())
    os.replace(temporary, directory / "control.json")


def metadata(path):
    if path == "/api/v3/suwar":
        return json.dumps(dict(suwar=[dict(id=i, name=f"Qualification surah {i}") for i in range(1, 115)])).encode()
    require(path == "/api/v3/reciters", "Unknown fixture metadata route")
    return json.dumps(dict(reciters=[dict(id=1, name="Private qualification recording", moshaf=[
        dict(id=1, name="Fixture edition", server="https://cdn.mp3quran.net/qualification/", surah_list="1")])])).encode()


def handler(directory):
    log_lock = threading.Lock()

    class Fixture(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # Never record credentials, arbitrary headers or client addresses.

        def _write_body(self, data, limit):
            # Each bounded write gets the socket timeout; an entire recording
            # may take longer while its reader continues making progress.
            with memoryview(data) as view:
                for offset in range(0, limit, BODY_CHUNK_BYTES):
                    self.wfile.write(view[offset:min(offset + BODY_CHUNK_BYTES, limit)])
            self.wfile.flush()

        def do_GET(self):
            try:
                require(self.path.startswith(PREFIX) and len(self.path) <= 1024, "Unknown route")
                logical = "https://" + self.path[len(PREFIX):]
                parsed = urlsplit(logical)
                meta = parsed.hostname == "www.mp3quran.net" and parsed.path in ("/api/v3/reciters", "/api/v3/suwar")
                audio = parsed.hostname == "cdn.mp3quran.net" and parsed.path == "/qualification/001.mp3" and not parsed.query
                require(meta or audio, "Unknown fixture route")
                control = json.loads((directory / "control.json").read_text())
                case = control["case"]
                require(case in CASES and 0 <= control["stall_seconds"] <= 45, "Invalid control")
                with log_lock:
                    log = directory / "requests.jsonl"
                    require(not log.is_symlink() and (not log.exists() or log.stat().st_size < 10 * 1024 * 1024), "Fixture log limit reached")
                    with log.open("a") as stream:
                        log.chmod(0o600)
                        stream.write(json.dumps(dict(utc=datetime.now(timezone.utc).isoformat(), logical_url=logical, case=case)) + "\n")
                phase = "metadata" if meta else "audio"
                if case == phase + "-status":
                    self.send_response(503); self.send_header("Content-Length", "0"); self.end_headers(); return
                if case.startswith(phase + "-redirect-"):
                    destination = (logical.replace("https:", "http:", 1) if case.endswith("http") else
                                   "https://evil.example/forbidden" if case.endswith("host") else logical)
                    self.send_response(302); self.send_header("Location", destination)
                    self.send_header("Content-Length", "0"); self.end_headers(); return
                if meta:
                    data = metadata(parsed.path)
                    if case == "metadata-malformed": data = b'{"reciters":[{"id":'
                    if case == "metadata-oversize": data = b'{"reciters":[{"name":"' + b'x' * 17000 + b'"}]}'
                    content_type = "application/json"
                elif case == "audio-decode":
                    data = b"Deliberately invalid MP3 input. No audio frames.\n"; content_type = "audio/mpeg"
                elif case == "audio-content-type":
                    data = b"Not a recording"; content_type = "text/plain"
                else:
                    path = Path(control["media"])
                    require(0 < path.stat().st_size <= MAX_MEDIA, "Invalid staged media")
                    data = path.read_bytes()
                    require(hashlib.sha256(data).hexdigest() == control["media_sha256"], "Media changed since validation")
                    content_type = "audio/mpeg"
                self.send_response(200); self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store"); self.end_headers()
                truncate = case == phase + "-truncate"
                stall = case == phase + "-stall"
                partial = max(1, len(data)//2)
                limit = partial if truncate else min(1024, partial) if stall else len(data)
                self._write_body(data, limit)
                if stall: time.sleep(control["stall_seconds"])
            except (OSError, ValueError, KeyError, TypeError):
                self.close_connection = True
            finally:
                self.close_connection = True
    return Fixture


def tls_server(address, directory, context):
    class TLSFixture(ThreadingHTTPServer):
        daemon_threads = True
        slots = threading.BoundedSemaphore(8)

        def get_request(self):
            connection, peer = super().get_request(); connection.settimeout(SOCKET_TIMEOUT_SECONDS)
            return connection, peer

        def process_request(self, connection, peer):
            if not self.slots.acquire(blocking=False):
                connection.close(); return
            try: super().process_request(connection, peer)
            except BaseException:
                self.slots.release(); connection.close(); raise

        def process_request_thread(self, connection, peer):
            try:
                try: secured = context.wrap_socket(connection, server_side=True)
                except (OSError, ssl.SSLError):
                    connection.close(); return
                # Bound request reads and each body chunk after TLS negotiation.
                secured.settimeout(SOCKET_TIMEOUT_SECONDS)
                super().process_request_thread(secured, peer)
            finally:
                self.slots.release()
    return TLSFixture(address, handler(directory))


def server(directory, tls_mode="trusted"):
    directory = private_directory(directory)
    require(tls_mode in ("trusted", "wrong-host", "untrusted"), "Invalid TLS fixture")
    config = json.loads((directory / "feed.json").read_text())
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory / f"server-{tls_mode}.pem", directory / "server-key.pem")
    return tls_server((lan_address(config["address"]), config["port"]), directory, context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action", required=True)
    init = actions.add_parser("init"); init.add_argument("directory", type=Path)
    init.add_argument("--address", required=True); init.add_argument("--port", type=int, default=8443)
    choose = actions.add_parser("select"); choose.add_argument("directory", type=Path)
    choose.add_argument("case", choices=CASES); choose.add_argument("--media", type=Path)
    choose.add_argument("--stall-seconds", type=float, default=40)
    serve = actions.add_parser("serve"); serve.add_argument("directory", type=Path)
    serve.add_argument("--tls-mode", choices=("trusted", "wrong-host", "untrusted"), default="trusted")
    args = parser.parse_args()
    os.umask(0o077)
    try:
        if args.action == "init": print(json.dumps(initialize(args.directory, args.address, args.port)))
        elif args.action == "select": select(args.directory, args.case, args.media, args.stall_seconds)
        else:
            with server(args.directory, args.tls_mode) as feed: feed.serve_forever()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()

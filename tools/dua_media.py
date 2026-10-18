"""Optional signed dua banks; recordings stay outside application images and Git."""
import hashlib
import json
import re
import struct
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.exceptions import InvalidSignature

BANK_START = 0x300000
BANK_SIZE = 0x40000
HEADER_SIZE = 4096
MAX_BYTES = BANK_SIZE - HEADER_SIZE
MAX_ENVELOPE = 2048
COMMIT = 0x314D4F43


def metadata(version, audio):
    if not re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version) or len(version) > 31 or any(int(part) > 0xffffffff for part in version[1:].split(".")) or not 0 < len(audio) <= MAX_BYTES:
        raise ValueError("Invalid dua version or recording size (maximum 252 KiB)")
    return dict(schema=1, kind="dua", version=version, mediaId="post-athan-dua-v1",
                layout="dual-2m-audio-3_5m-v1", format=1, bytes=len(audio), sha256=hashlib.sha256(audio).hexdigest())


def strict_json(data):
    def unique(pairs):
        out = {}
        for key, value in pairs:
            if key in out:
                raise ValueError("Duplicate dua metadata key")
            out[key] = value
        return out
    return json.loads(data, object_pairs_hook=unique)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def sign(version, audio, private_pem, public_pem):
    private = serialization.load_pem_private_key(private_pem, password=None)
    public = serialization.load_pem_public_key(public_pem)
    if not isinstance(private, ec.EllipticCurvePrivateKey) or not isinstance(private.curve, ec.SECP256R1) or \
            not isinstance(public, ec.EllipticCurvePublicKey) or private.public_key().public_numbers() != public.public_numbers():
        raise ValueError("Dua signing key differs from production P-256 trust")
    payload = canonical(metadata(version, audio))
    envelope = canonical(dict(payload=payload.decode(), signature=private.sign(payload, ec.ECDSA(hashes.SHA256())).hex()))
    validate(envelope, audio, public_pem, version)
    return envelope


def validate(envelope, audio, public_pem, version=None):
    if not 0 < len(envelope) <= MAX_ENVELOPE:
        raise ValueError("Oversized dua descriptor")
    wrapper = strict_json(envelope)
    if set(wrapper) != {"payload", "signature"} or not isinstance(wrapper["payload"], str) or len(wrapper["payload"].encode()) > 1024:
        raise ValueError("Invalid dua envelope")
    payload = wrapper["payload"].encode()
    signature = bytes.fromhex(wrapper["signature"])
    key = serialization.load_pem_public_key(public_pem)
    if not isinstance(key, ec.EllipticCurvePublicKey) or not isinstance(key.curve, ec.SECP256R1) or not 64 <= len(signature) <= 72:
        raise ValueError("Invalid dua signature")
    try:
        key.verify(signature, payload, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature as error:
        raise ValueError("Dua signature verification failed") from error
    doc = strict_json(payload)
    expected = metadata(version or doc.get("version", ""), audio)
    if doc != expected or payload != canonical(expected):
        raise ValueError("Dua descriptor does not match recording")
    return doc


def bank(audio, envelope, generation=1):
    if not 0 < len(audio) <= MAX_BYTES or not 0 < len(envelope) <= MAX_ENVELOPE or not 0 < generation <= 0xffffffff:
        raise ValueError("Invalid dua bank bounds")
    out = bytearray(b"\xff" * BANK_SIZE)
    struct.pack_into("<8sIIII32s", out, 0, b"OADUA001", generation, len(audio), len(envelope), 1, hashlib.sha256(audio).digest())
    out[64:64 + len(envelope)] = envelope
    out[4060:4092] = hashlib.sha256(out[:4060]).digest()
    struct.pack_into("<I", out, 4092, COMMIT)
    out[4096:4096 + len(audio)] = audio
    return bytes(out)


def read_bank(data, public_pem):
    if len(data) != BANK_SIZE:
        raise ValueError("Invalid dua bank size")
    magic, generation, length, size, fmt, digest = struct.unpack_from("<8sIIII32s", data)
    if magic != b"OADUA001" or not generation or fmt != 1 or not 0 < length <= MAX_BYTES or not 0 < size <= MAX_ENVELOPE or \
            struct.unpack_from("<I", data, 4092)[0] != COMMIT or hashlib.sha256(data[:4060]).digest() != data[4060:4092]:
        raise ValueError("Invalid or uncommitted dua bank")
    audio, envelope = data[4096:4096 + length], data[64:64 + size]
    if hashlib.sha256(audio).digest() != digest:
        raise ValueError("Dua payload hash mismatch")
    validate(envelope, audio, public_pem)
    if bank(audio, envelope, generation) != data:
        raise ValueError("Noncanonical dua bank")
    return audio, envelope, generation

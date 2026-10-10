"""Signed, application-only upgrade assets. Private keys never enter bundles."""
import json
import struct
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.exceptions import InvalidSignature
from release_artifacts import (HARDWARE, LAYOUT, SHA, digest, esp_image,
                               json_bytes, read_json, require, profile)

UPGRADE_ASSETS = ("firmware.ota.bin", "upgrade.json")


def image_version(app, hardware=HARDWARE):
    esp_image(app, exact=True, flash_bytes=profile(hardware)["flash_bytes"])
    require(len(app) >= 288 and struct.unpack_from("<I", app, 32)[0] == 0xabcd5432,
            "Application has no ESP-IDF description")
    return app[48:80].split(b"\0", 1)[0].decode("ascii")


def metadata(version, commit, app, hardware=HARDWARE):
    # Keep this contract aligned with the device verifier; URLs are derived
    # from an authenticated version rather than supplied by the descriptor.
    import re
    require(re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version), "Invalid stable version")
    require(len(version) <= 31 and all(int(part) <= 0xffffffff for part in version[1:].split('.')),
            "Stable version exceeds the firmware identity limits")
    require(SHA.fullmatch(commit), "Invalid source commit")
    require(256 <= len(app) <= 1572864, "Application exceeds its budget")
    require(image_version(app, hardware) == version, "Embedded application version differs from release")
    return dict(schema=1, version=version, commit=commit, hardware=hardware, layout=LAYOUT,
                storageFormat=1, audioFormat=1, rollback=True, bytes=len(app), sha256=digest(app))


def public_key(pem):
    key = serialization.load_pem_public_key(pem)
    require(isinstance(key, ec.EllipticCurvePublicKey) and isinstance(key.curve, ec.SECP256R1),
            "Upgrade key must be ECDSA P-256")
    return key


def sign(version, commit, app, private_pem, public_pem, hardware=HARDWARE):
    key = serialization.load_pem_private_key(private_pem, password=None)
    public = public_key(public_pem)
    require(isinstance(key, ec.EllipticCurvePrivateKey) and isinstance(key.curve, ec.SECP256R1) and
            key.public_key().public_numbers() == public.public_numbers(), "Signing key differs from firmware trust key")
    payload = json_bytes(metadata(version, commit, app, hardware))
    envelope = json_bytes(dict(payload=payload.decode(), signature=key.sign(payload, ec.ECDSA(hashes.SHA256())).hex()))
    validate(envelope, app, public_pem, version, commit, hardware)
    return envelope


def validate(envelope, app, public_pem, version, commit, hardware=HARDWARE):
    require(0 < len(envelope) <= 8192, "Oversized upgrade descriptor")
    wrapper = read_json(envelope)
    require(isinstance(wrapper, dict) and set(wrapper) == {"payload", "signature"} and
            isinstance(wrapper["payload"], str) and isinstance(wrapper["signature"], str), "Invalid signature envelope")
    payload = wrapper["payload"].encode()
    require(0 < len(payload) <= 4096, "Oversized upgrade payload")
    try:
        signature = bytes.fromhex(wrapper["signature"])
        require(64 <= len(signature) <= 72, "Invalid ECDSA signature length")
        public_key(public_pem).verify(signature, payload, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature as error:
        raise ValueError("Upgrade signature verification failed") from error
    expected = metadata(version, commit, app, hardware)
    require(read_json(payload) == expected and payload == json_bytes(expected), "Upgrade descriptor differs from firmware")
    return expected

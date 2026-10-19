"""Private dua media qualification; firmware update transports stay disabled."""

import ipaddress
from pathlib import Path
from urllib.parse import urlsplit
from esphome import codegen as cg, config_validation as cv, final_validate as fv

DEPENDENCIES = ["openathan_device", "openathan_provisioning_validation"]


def origin(value):
    try:
        parsed = urlsplit(value)
        address = ipaddress.IPv4Address(parsed.hostname)
        private = any(
            address in ipaddress.IPv4Network(network)
            for network in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")
        )
        if (
            parsed.scheme == "https"
            and private
            and parsed.port is not None
            and 1024 <= parsed.port <= 65535
            and value == f"https://{address}:{parsed.port}"
        ):
            return value
    except (ValueError, TypeError):
        pass
    raise cv.Invalid(
        "Dua qualification requires https://PRIVATE_IPV4:PORT without a path"
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required("origin"): origin,
        cv.Required("ca_file"): cv.file_,
        cv.Required("public_key_file"): cv.file_,
        cv.Required("version"): cv.one_of("v0.0.1"),
    }
)


def validate(config):
    full = fv.full_config.get()
    if (
        "openathan_provisioning_validation" not in full
        or full["esphome"]["name"] != "openathan-test"
        or not full["esphome"]["name_add_mac_suffix"]
        or full["esphome"].get("project", {}).get("version") != config["version"]
        or full["openathan_device"].get("updates_enabled") is not False
        or any(
            name in full
            for name in (
                "openathan_upgrade_qualification",
                "openathan_quran_qualification",
            )
        )
    ):
        raise cv.Invalid(
            "Dua qualification requires isolated openathan-test, matching version and disabled updates"
        )
    return config


FINAL_VALIDATE_SCHEMA = validate


async def to_code(config):
    from cryptography import x509
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec

    ca = Path(config["ca_file"]).read_text()
    public = Path(config["public_key_file"]).read_text()
    try:
        if (
            "PRIVATE KEY" in ca
            or "PRIVATE KEY" in public
            or len(ca.encode()) > 4096
            or ca.count("-----BEGIN CERTIFICATE-----") != 1
        ):
            raise ValueError("expected one public CA and public signing key")
        certificate = x509.load_pem_x509_certificate(ca.encode())
        if not certificate.extensions.get_extension_for_class(
            x509.BasicConstraints
        ).value.ca:
            raise ValueError("not a CA")
        key = serialization.load_pem_public_key(public.encode())
        root = Path(__file__).resolve().parents[4]
        production = serialization.load_pem_public_key(
            (root / "release/upgrade-public-key.pem").read_bytes()
        )
        if (
            not isinstance(key, ec.EllipticCurvePublicKey)
            or not isinstance(key.curve, ec.SECP256R1)
            or key.public_numbers() == production.public_numbers()
        ):
            raise ValueError("expected a private-qualification P-256 public key")
    except (ValueError, x509.ExtensionNotFound) as error:
        raise cv.Invalid(f"Invalid dua qualification trust: {error}")
    cg.add_define("OPENATHAN_DUA_QUALIFICATION")
    cg.add_define("OPENATHAN_DUA_QUALIFICATION_ORIGIN", config["origin"])
    cg.add_define("OPENATHAN_DUA_QUALIFICATION_CA", ca)
    cg.add_define("OPENATHAN_DUA_QUALIFICATION_PUBLIC_KEY", public)

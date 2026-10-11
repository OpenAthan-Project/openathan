"""Maintainer-only Quran transport, never included in release firmware."""
from pathlib import Path
import ipaddress
from urllib.parse import urlsplit

from esphome import codegen as cg, config_validation as cv, final_validate as fv

DEPENDENCIES = ["openathan_device", "openathan_provisioning_validation"]


def origin(value):
    value = cv.string_strict(value)
    try:
        parsed = urlsplit(value)
        address = ipaddress.IPv4Address(parsed.hostname)
        port = parsed.port
    except (ValueError, TypeError):
        raise cv.Invalid("Use https://PRIVATE_IPV4:PORT without a path")
    private = any(address in ipaddress.IPv4Network(network) for network in
                  ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))
    if (not private or port is None or not 1024 <= port <= 65535 or
            value != f"https://{address}:{port}"):
        raise cv.Invalid("Use https://PRIVATE_IPV4:PORT without a path")
    return value


CONFIG_SCHEMA = cv.Schema({cv.Required("origin"): origin, cv.Required("ca_file"): cv.file_})


def validate(config):
    full = fv.full_config.get()
    if ("openathan_provisioning_validation" not in full or
            full.get("esphome", {}).get("name") != "openathan-test" or
            not full.get("esphome", {}).get("name_add_mac_suffix", False) or
            full.get("openathan_device", {}).get("updates_enabled", True) or
            "openathan_upgrade_qualification" in full):
        raise cv.Invalid("Quran qualification requires isolated openathan-test firmware with updates disabled")
    return config


FINAL_VALIDATE_SCHEMA = validate


async def to_code(config):
    from cryptography import x509
    ca = Path(config["ca_file"]).read_bytes()
    try:
        text = ca.decode("ascii")
        if len(ca) > 4096 or b"PRIVATE KEY" in ca or ca.count(b"-----BEGIN CERTIFICATE-----") != 1:
            raise ValueError("embed only a bounded public CA certificate")
        certificate = x509.load_pem_x509_certificate(ca)
        if not certificate.extensions.get_extension_for_class(x509.BasicConstraints).value.ca:
            raise ValueError("not a CA")
    except (ValueError, x509.ExtensionNotFound) as error:
        raise cv.Invalid(f"Invalid Quran qualification CA: {error}")
    cg.add_define("OPENATHAN_QURAN_QUALIFICATION")
    cg.add_define("OPENATHAN_QURAN_QUALIFICATION_ORIGIN", config["origin"])
    cg.add_define("OPENATHAN_QURAN_QUALIFICATION_CA", text)

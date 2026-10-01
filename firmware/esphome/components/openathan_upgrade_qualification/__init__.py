"""Maintainer-only OTA qualification, always using isolated provisioning records."""
import ipaddress
from pathlib import Path
from urllib.parse import urlsplit

from esphome import codegen as cg, config_validation as cv, final_validate as fv

DEPENDENCIES = ["openathan_device", "openathan_provisioning_validation"]


def origin(value):
    try:
        parsed = urlsplit(value)
        address = ipaddress.IPv4Address(parsed.hostname)
        port = parsed.port
    except (ValueError, TypeError):
        raise cv.Invalid("Qualification requires a private LAN IPv4 HTTPS origin")
    private = any(address in ipaddress.IPv4Network(network) for network in
                  ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))
    if (parsed.scheme != "https" or not private or
            port is None or not 1024 <= port <= 65535 or parsed.username is not None or parsed.password is not None or
            parsed.path or parsed.query or parsed.fragment or value != f"https://{address}:{port}"):
        raise cv.Invalid("Qualification requires https://PRIVATE_IPV4:PORT without a path")
    return f"https://{address}:{port}"


CONFIG_SCHEMA = cv.Schema({
    cv.Required("origin"): origin,
    cv.Required("ca_file"): cv.file_,
    cv.Required("public_key_file"): cv.file_,
    cv.Required("version"): cv.one_of("v0.0.1", "v0.0.2", "v0.0.3", "v0.0.4"),
    cv.Optional("startup_failure", default=False): cv.boolean,
})


def validate(config):
    full = fv.full_config.get()
    if ("openathan_provisioning_validation" not in full or
            full["esphome"]["name"] != "openathan-test" or
            not full["esphome"]["name_add_mac_suffix"] or
            full["esphome"].get("project", {}).get("version") != config["version"]):
        raise cv.Invalid("Upgrade qualification requires isolated openathan-test firmware and matching version")
    if config["startup_failure"] != (config["version"] == "v0.0.3"):
        raise cv.Invalid("Only v0.0.3 must inject startup-health failure")
    return config


FINAL_VALIDATE_SCHEMA = validate


def firmware_identity(config):
    """Override identity only for the explicitly selected qualification build."""
    return config["version"], Path(config["public_key_file"]).read_text()


async def to_code(config):
    from cryptography import x509
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    ca = Path(config["ca_file"]).read_text()
    public = Path(config["public_key_file"]).read_text()
    try:
        if "PRIVATE KEY" in ca or "PRIVATE KEY" in public:
            raise ValueError("private key material must never be embedded")
        certificate = x509.load_pem_x509_certificate(ca.encode())
        if not certificate.extensions.get_extension_for_class(x509.BasicConstraints).value.ca:
            raise ValueError("not a CA")
        key = serialization.load_pem_public_key(public.encode())
        if not isinstance(key, ec.EllipticCurvePublicKey) or not isinstance(key.curve, ec.SECP256R1):
            raise ValueError("not P-256")
        root = Path(__file__).resolve().parents[4]
        production = serialization.load_pem_public_key((root / "release/upgrade-public-key.pem").read_bytes())
        if key.public_numbers() == production.public_numbers():
            raise ValueError("production trust key")
    except (ValueError, x509.ExtensionNotFound) as error:
        raise cv.Invalid(f"Invalid qualification trust material: {error}")
    cg.add_define("OPENATHAN_UPGRADE_QUALIFICATION")
    cg.add_define("OPENATHAN_QUALIFICATION_ORIGIN", config["origin"])
    cg.add_define("OPENATHAN_QUALIFICATION_CA", ca)
    cg.add_define("OPENATHAN_QUALIFICATION_STARTUP_FAILURE", config["startup_failure"])

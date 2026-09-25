"""Explicit opt-in to isolated storage; never included by production firmware."""
from esphome import codegen as cg, config_validation as cv, final_validate as fv

DEPENDENCIES = ["openathan_device"]
CONFIG_SCHEMA = cv.Schema({})


def validate(config):
    full = fv.full_config.get()
    if full["esphome"]["name"] != "openathan-test" or not full["esphome"]["name_add_mac_suffix"]:
        raise cv.Invalid("Provisioning validation requires openathan-test with a MAC suffix")
    if "openathan_validation" in full:
        raise cv.Invalid("Provisioning validation uses real calculations, not the synthetic scheduler")
    return config


FINAL_VALIDATE_SCHEMA = validate


async def to_code(config):
    cg.add_define("OPENATHAN_PROVISIONING_TEST_STORAGE")

"""Reference-image USB transport, replacing the stock serial reader.

Improv framing stays standard. OpenAthan password commands use a separate header.
Official product configurations load this external component.
"""
from esphome import codegen as cg, config_validation as cv, final_validate as fv
from esphome.components import openathan_device
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["logger", "wifi", "openathan_device"]
ns = cg.esphome_ns.namespace("improv_serial")
ImprovSerialComponent = ns.class_("ImprovSerialComponent", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ImprovSerialComponent),
    cv.Required("device_id"): cv.use_id(openathan_device.Device),
}).extend(cv.COMPONENT_SCHEMA)


def validate(config):
    full = fv.full_config.get()
    logger = full["logger"]
    if logger["hardware_uart"] != "USB_SERIAL_JTAG":
        raise cv.Invalid("OpenAthan USB provisioning requires USB_SERIAL_JTAG")
    encrypted_diagnostics = (logger["baud_rate"] == 0 and "openathan_provisioning_validation" in full
                             and "encryption" in full.get("api", {}))
    if logger["level"] != "NONE" and not encrypted_diagnostics:
        raise cv.Invalid("OpenAthan provisioning owns serial output; set logger level NONE")
    return config


FINAL_VALIDATE_SCHEMA = validate


async def to_code(config):
    if CORE.config["logger"]["baud_rate"] == 0:
        cg.add_define("OPENATHAN_USB_OWNS_DRIVER")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device(await cg.get_variable(config["device_id"])))
    cg.add_define("USE_IMPROV_SERIAL")

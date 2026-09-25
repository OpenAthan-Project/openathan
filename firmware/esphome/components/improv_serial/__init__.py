"""Reference-image USB transport, replacing the stock serial reader.

Improv framing stays standard. OpenAthan password commands use a separate header.
Only the official reference configuration loads this external component.
"""
from esphome import codegen as cg, config_validation as cv, final_validate as fv
from esphome.components import openathan_device
from esphome.const import CONF_ID

DEPENDENCIES = ["logger", "wifi", "openathan_device"]
ns = cg.esphome_ns.namespace("improv_serial")
ImprovSerialComponent = ns.class_("ImprovSerialComponent", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ImprovSerialComponent),
    cv.Required("device_id"): cv.use_id(openathan_device.Device),
}).extend(cv.COMPONENT_SCHEMA)


def validate(config):
    logger = fv.full_config.get()["logger"]
    if logger["hardware_uart"] != "USB_SERIAL_JTAG":
        raise cv.Invalid("OpenAthan USB provisioning requires USB_SERIAL_JTAG")
    if logger["level"] != "NONE":
        raise cv.Invalid("OpenAthan provisioning owns serial output; set logger level NONE")
    return config


FINAL_VALIDATE_SCHEMA = validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device(await cg.get_variable(config["device_id"])))
    cg.add_define("USE_IMPROV_SERIAL")

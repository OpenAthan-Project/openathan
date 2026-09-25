import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
ns = cg.esphome_ns.namespace("voice_pyramid")
Clock = ns.class_("Clock", cg.Component, i2c.I2CDevice)
Amplifier = ns.class_("Amplifier", cg.Component, i2c.I2CDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.Required("clock"): cv.Schema({cv.GenerateID(): cv.declare_id(Clock)})
        .extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x60)),
    cv.Required("amplifier"): cv.Schema({cv.GenerateID(): cv.declare_id(Amplifier)})
        .extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x5B)),
})


async def to_code(config):
    for key in ("clock", "amplifier"):
        var = cg.new_Pvariable(config[key][CONF_ID])
        await cg.register_component(var, config[key])
        await i2c.register_i2c_device(var, config[key])

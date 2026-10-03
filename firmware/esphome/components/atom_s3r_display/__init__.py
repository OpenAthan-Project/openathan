from esphome import codegen as cg, config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
ns = cg.esphome_ns.namespace("atom_s3r_display")
Backlight = ns.class_("Backlight", cg.Component, i2c.I2CDevice)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Backlight),
    cv.Optional("brightness_percent", default=10): cv.int_range(min=1, max=100),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x30))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_brightness(config["brightness_percent"]))

from esphome import codegen as cg, config_validation as cv
from esphome.components import i2c, time
from esphome.components.sntp.time import SNTPComponent
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c", "openathan"]
ns = cg.esphome_ns.namespace("waveshare_box_rtc")
RTC = ns.class_("RTC", time.RealTimeClock, i2c.I2CDevice)
CONFIG_SCHEMA = time.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(RTC),
    cv.Required("network_time_id"): cv.use_id(SNTPComponent),
}).extend(i2c.i2c_device_schema(0x51)).extend(cv.polling_component_schema("never"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
    cg.add(var.set_network_time(await cg.get_variable(config["network_time_id"])))

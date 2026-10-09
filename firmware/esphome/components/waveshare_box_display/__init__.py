from esphome import codegen as cg, config_validation as cv
from esphome.components import display, output
from esphome.const import CONF_ID

DEPENDENCIES = ["display", "output"]
ns = cg.esphome_ns.namespace("waveshare_box_display")
Backlight = ns.class_("Backlight", cg.Component, cg.global_ns.namespace("openathan").class_("DisplayOutput"))
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Backlight),
    cv.Required("output_id"): cv.use_id(output.FloatOutput),
    cv.Required("display_id"): cv.use_id(display.Display),
    cv.Optional("brightness_percent", default=50): cv.int_range(min=1, max=100),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_output(await cg.get_variable(config["output_id"])))
    cg.add(var.set_display(await cg.get_variable(config["display_id"])))
    cg.add(var.set_brightness(config["brightness_percent"]))

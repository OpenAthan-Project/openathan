from esphome import codegen as cg, config_validation as cv
from esphome.components import display, openathan
from esphome.const import CONF_ID

DEPENDENCIES = ["openathan", "display", "wifi"]
ns = cg.esphome_ns.namespace("openathan_display")
StatusDisplay = ns.class_("StatusDisplay", cg.PollingComponent)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(StatusDisplay),
    cv.Required("openathan_id"): cv.use_id(openathan.OpenAthan),
    cv.Required("display_id"): cv.use_id(display.Display),
    cv.Optional("layout", default="square_128"): cv.one_of("square_128", "round_360"),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_openathan(await cg.get_variable(config["openathan_id"])))
    cg.add(var.set_display(await cg.get_variable(config["display_id"])))
    cg.add(var.set_round(config["layout"] == "round_360"))

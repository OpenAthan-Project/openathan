"""Developer-only synthetic timetable; never loaded by production packages."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.openathan import OpenAthan
from esphome.const import CONF_ID

DEPENDENCIES = ["openathan"]
ns = cg.esphome_ns.namespace("openathan_validation")
Validation = ns.class_("Validation", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Validation),
    cv.Required("scheduler_id"): cv.use_id(OpenAthan),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_scheduler(await cg.get_variable(config["scheduler_id"])))

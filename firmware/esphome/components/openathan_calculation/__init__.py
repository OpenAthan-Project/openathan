from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]
ns = cg.esphome_ns.namespace("openathan_calculation")
Calculation = ns.class_("Calculation", cg.Component)
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(Calculation)}).extend(cv.COMPONENT_SCHEMA)


def register_core():
    esp32.add_idf_component(name="openathan-core", path=str(Path(__file__).resolve().parents[4] / "lib/openathan-core"))
    # Retain released Adhan source unchanged, including its checked exceptions.
    esp32.add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)


async def to_code(config):
    register_core()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

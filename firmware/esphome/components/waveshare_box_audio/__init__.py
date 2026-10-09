from esphome import codegen as cg, config_validation as cv, pins
from esphome.components import openathan_audio
from esphome.components.es8311.audio_dac import ES8311
from esphome.const import CONF_ID

DEPENDENCIES = ["audio_dac", "openathan_audio"]
ns = cg.esphome_ns.namespace("waveshare_box_audio")
Audio = ns.class_("Audio", cg.Component, cg.global_ns.namespace("openathan").class_("Playback"))
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Audio),
    cv.Required("playback_id"): cv.use_id(openathan_audio.PartitionAudio),
    cv.Required("dac_id"): cv.use_id(ES8311),
    cv.Required("amplifier_pin"): pins.internal_gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_playback(await cg.get_variable(config["playback_id"])))
    cg.add(var.set_dac(await cg.get_variable(config["dac_id"])))
    cg.add(var.set_amplifier(await cg.gpio_pin_expression(config["amplifier_pin"])))

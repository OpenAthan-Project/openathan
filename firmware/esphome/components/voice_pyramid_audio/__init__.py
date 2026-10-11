import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.voice_pyramid import Clock, Amplifier
from esphome.const import CONF_ID

DEPENDENCIES = ["voice_pyramid", "openathan", "openathan_audio", "audio_dac", "switch"]
ns = cg.esphome_ns.namespace("voice_pyramid_audio")
Audio = ns.class_("Audio", cg.Component, cg.global_ns.namespace("openathan").class_("Playback"))
Playback = cg.esphome_ns.namespace("openathan_audio").class_("PartitionAudio")
ES8311 = cg.esphome_ns.namespace("es8311").class_("ES8311")
Switch = cg.esphome_ns.namespace("switch_").class_("Switch")
OpenAthan = cg.esphome_ns.namespace("openathan_component").class_("OpenAthan")
Validation = cg.esphome_ns.namespace("openathan_validation").class_("Validation")
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Audio),
    cv.Required("playback_id"): cv.use_id(Playback),
    cv.Required("dac_id"): cv.use_id(ES8311),
    cv.Required("enable_id"): cv.use_id(Switch),
    cv.Required("clock_id"): cv.use_id(Clock),
    cv.Required("amplifier_id"): cv.use_id(Amplifier),
    cv.Required("scheduler_id"): cv.use_id(OpenAthan),
    cv.Optional("validation_id"): cv.use_id(Validation),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = (cg.new_Pvariable(config[CONF_ID], "oa_validation") if "validation_id" in config
           else cg.new_Pvariable(config[CONF_ID]))
    await cg.register_component(var, config)
    for key in ("playback", "dac", "enable", "clock", "amplifier"):
        cg.add(getattr(var, "set_" + key)(await cg.get_variable(config[key + "_id"])))
    scheduler = await cg.get_variable(config["scheduler_id"])
    cg.add(scheduler.set_settings_store(var.settings_store()))
    # Equivalent to the old 70% startup setting, rounded slightly quieter.
    cg.add(scheduler.set_default_volume(56))
    if "validation_id" in config:
        validation = await cg.get_variable(config["validation_id"])
        cg.add(validation.set_settings_store(var.settings_store()))

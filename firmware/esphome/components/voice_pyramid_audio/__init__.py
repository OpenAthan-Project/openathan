import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.voice_pyramid import Clock, Amplifier
from esphome.const import CONF_ID

DEPENDENCIES = ["voice_pyramid", "openathan", "openathan_audio", "audio_dac", "i2s_audio"]
ns = cg.esphome_ns.namespace("voice_pyramid_audio")
Audio = ns.class_("Audio", cg.Component, cg.global_ns.namespace("openathan").class_("Playback"))
Playback = cg.esphome_ns.namespace("openathan_audio").class_("PartitionAudio")
ES8311 = cg.esphome_ns.namespace("es8311").class_("ES8311")
from esphome.components.i2s_audio.speaker import I2SAudioSpeakerBase
from esphome import final_validate as fv
OpenAthan = cg.esphome_ns.namespace("openathan_component").class_("OpenAthan")
Validation = cg.esphome_ns.namespace("openathan_validation").class_("Validation")
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Audio),
    cv.Required("playback_id"): cv.use_id(Playback),
    cv.Required("dac_id"): cv.use_id(ES8311),
    cv.Required("speaker_id"): cv.use_id(I2SAudioSpeakerBase),
    cv.Required("clock_id"): cv.use_id(Clock),
    cv.Required("amplifier_id"): cv.use_id(Amplifier),
    cv.Required("scheduler_id"): cv.use_id(OpenAthan),
    cv.Optional("validation_id"): cv.use_id(Validation),
}).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config):
    full = fv.full_config.get()
    path = full.get_path_for_id(config["speaker_id"])[:-1]
    if not full.get_config_for_path(path).get("volume_fence"):
        raise cv.Invalid("voice_pyramid_audio requires volume_fence on its speaker")
    amp_path = full.get_path_for_id(config["amplifier_id"])[:-1]
    if full.get_config_for_path(amp_path).get("enable_on_startup", True):
        raise cv.Invalid("voice_pyramid_audio requires amplifier enable_on_startup: false")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = (cg.new_Pvariable(config[CONF_ID], "oa_validation") if "validation_id" in config
           else cg.new_Pvariable(config[CONF_ID]))
    await cg.register_component(var, config)
    for key in ("playback", "dac", "speaker", "clock", "amplifier"):
        cg.add(getattr(var, "set_" + key)(await cg.get_variable(config[key + "_id"])))
    scheduler = await cg.get_variable(config["scheduler_id"])
    cg.add(scheduler.set_settings_store(var.settings_store()))
    # Equivalent to the old 70% startup setting, rounded slightly quieter.
    cg.add(scheduler.set_default_volume(56))
    if "validation_id" in config:
        validation = await cg.get_variable(config["validation_id"])
        cg.add(validation.set_settings_store(var.settings_store()))

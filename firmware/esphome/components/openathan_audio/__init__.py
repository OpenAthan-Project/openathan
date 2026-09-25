from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.components.speaker.media_player import SpeakerMediaPlayer
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "speaker"]
ns = cg.esphome_ns.namespace("openathan_audio")
Playback = cg.global_ns.namespace("openathan").class_("Playback")
PartitionAudio = ns.class_("PartitionAudio", cg.Component, Playback)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PartitionAudio),
    cv.Required("media_player_id"): cv.use_id(SpeakerMediaPlayer),
    cv.Optional("partition", default="athan_audio"): cv.string_strict,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    esp32.add_idf_component(name="openathan-core", path=str(Path(__file__).resolve().parents[4] / "lib/openathan-core"))
    esp32.add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    esp32.include_builtin_idf_component("esp_partition")
    esp32.include_builtin_idf_component("mbedtls")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_player(await cg.get_variable(config["media_player_id"])))
    cg.add(var.set_partition(config["partition"]))

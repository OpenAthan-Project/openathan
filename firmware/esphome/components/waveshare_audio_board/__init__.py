from esphome import codegen as cg, config_validation as cv
from esphome.components import i2c, openathan, openathan_audio, light
from esphome.components.es8311.audio_dac import ES8311
from esphome.components.esp32_rmt_led_strip.light import ESP32RMTLEDStripLightOutput
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c", "openathan", "openathan_audio", "audio_dac", "light"]
AUTO_LOAD = ["es8311_playback"]
ns = cg.esphome_ns.namespace("waveshare_audio_board")
Board = ns.class_("Board", cg.Component, i2c.I2CDevice)
Audio = ns.class_("Audio", cg.Component, openathan.Playback)
Ring = ns.class_("Ring", cg.Component, openathan.LightOutput)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Board),
    cv.GenerateID("audio_id"): cv.declare_id(Audio),
    cv.GenerateID("ring_id"): cv.declare_id(Ring),
    cv.Required("openathan_id"): cv.use_id(openathan.OpenAthan),
    cv.Required("playback_id"): cv.use_id(openathan_audio.PartitionAudio),
    cv.Required("dac_id"): cv.use_id(ES8311),
    cv.Required("light_id"): cv.use_id(light.LightState),
    cv.Required("led_driver_id"): cv.use_id(ESP32RMTLEDStripLightOutput),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x20))


async def to_code(config):
    board = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(board, config)
    await i2c.register_i2c_device(board, config)
    controller = await cg.get_variable(config["openathan_id"])
    cg.add(board.set_key_handler(cg.RawExpression(
        f"[](unsigned key) {{ if (key == 1) {controller}->stop(); "
        f"else {controller}->adjust_volume(key == 0 ? -5 : 5); }}")))
    audio = cg.new_Pvariable(config["audio_id"])
    await cg.register_component(audio, config)
    cg.add(audio.set_board(board))
    cg.add(audio.set_playback(await cg.get_variable(config["playback_id"])))
    cg.add(audio.set_dac(await cg.get_variable(config["dac_id"])))
    ring = cg.new_Pvariable(config["ring_id"])
    await cg.register_component(ring, config)
    cg.add(ring.set_light(await cg.get_variable(config["light_id"])))
    cg.add(ring.set_driver(await cg.get_variable(config["led_driver_id"])))

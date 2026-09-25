from pathlib import Path
import math

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import esp32, time
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "time"]
AUTO_LOAD = ["json"]
core = cg.global_ns.namespace("openathan")
Playback = core.class_("Playback")
Method = core.enum("Method", is_class=True)
HighLatitudeRule = core.enum("HighLatitudeRule", is_class=True)
ns = cg.esphome_ns.namespace("openathan_component")
OpenAthan = ns.class_("OpenAthan", cg.PollingComponent)
ControlAction = ns.class_("ControlAction", automation.Action)
METHODS = {name.lower(): getattr(Method, name) for name in (
    "MUSLIM_WORLD_LEAGUE", "EGYPTIAN", "KARACHI", "UMM_AL_QURA", "DUBAI",
    "MOONSIGHTING_COMMITTEE", "NORTH_AMERICA", "KUWAIT", "QATAR", "SINGAPORE", "TEHRAN", "TURKEY")}
HIGH_LATITUDE = {name.lower(): getattr(HighLatitudeRule, name) for name in (
    "MIDDLE_OF_NIGHT", "SEVENTH_OF_NIGHT", "TWILIGHT_ANGLE", "AUTO")}
PRAYERS = ("fajr", "dhuhr", "asr", "maghrib", "isha")
EVENTS = ("fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha")


def coordinate(low, high):
    def validate(value):
        value = cv.float_(value)
        if not math.isfinite(value) or not low <= value <= high:
            raise cv.Invalid(f"Coordinate must be finite and in [{low}, {high}]")
        return value
    return validate


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OpenAthan),
    cv.Required("time_id"): cv.use_id(time.RealTimeClock),
    cv.Required("playback_id"): cv.use_id(Playback),
    cv.Required("latitude"): coordinate(-90, 90),
    cv.Required("longitude"): coordinate(-180, 180),
    cv.Required("method"): cv.enum(METHODS, lower=True),
    cv.Optional("timezone_name"): cv.All(cv.string_strict, cv.Length(min=1, max=96)),
    cv.Optional("hanafi", default=False): cv.boolean,
    cv.Optional("high_latitude", default="auto"): cv.enum(HIGH_LATITUDE, lower=True),
    cv.Optional("offsets", default={}): cv.Schema({
        cv.Optional(name, default=0): cv.int_range(min=-120, max=120) for name in EVENTS}),
    cv.Optional("enabled", default={}): cv.Schema({
        cv.Optional(name, default=True): cv.boolean for name in PRAYERS}),
}).extend(cv.COMPONENT_SCHEMA)


def validate_timezone(config):
    full = fv.full_config.get()
    clock = full.get_config_for_path(full.get_path_for_id(config["time_id"])[:-1])
    if not clock.get("timezone"):
        raise cv.Invalid("OpenAthan requires an explicit timezone on its time source")
    # ESPHome's parsed timezone is global, even when multiple clock sources exist.
    if any(source.get("timezone") != clock["timezone"] for source in full.get("time", [])):
        raise cv.Invalid("All ESPHome time sources must use the same explicit timezone as OpenAthan")
    if "timezone_name" in config and time.validate_tz(config["timezone_name"]) != clock["timezone"]:
        raise cv.Invalid("timezone_name must resolve to the configured clock timezone")
    config.setdefault("timezone_name", clock["timezone"])
    return config


FINAL_VALIDATE_SCHEMA = validate_timezone


async def to_code(config):
    esp32.add_idf_component(name="openathan-core", path=str(Path(__file__).resolve().parents[4] / "lib/openathan-core"))
    esp32.add_idf_sdkconfig_option("CONFIG_COMPILER_CXX_EXCEPTIONS", True)
    esp32.include_builtin_idf_component("nvs_flash")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_clock(await cg.get_variable(config["time_id"])))
    cg.add(var.set_default_timezone(config["timezone_name"]))
    cg.add(var.set_playback(await cg.get_variable(config["playback_id"])))
    for field in ("latitude", "longitude", "method", "hanafi", "high_latitude"):
        cg.add(getattr(var, "set_" + field)(config[field]))
    for index, name in enumerate(EVENTS):
        cg.add(var.set_offset(index, config["offsets"][name]))
    for index, name in enumerate(PRAYERS):
        cg.add(var.set_enabled(index, config["enabled"][name]))


def register_control(name, kind):
    @automation.register_action(name, ControlAction, cv.Schema({cv.Required(CONF_ID): cv.use_id(OpenAthan)}), synchronous=True)
    async def to_code(config, action_id, template_arg, args):
        parent = await cg.get_variable(config[CONF_ID])
        return cg.new_Pvariable(action_id, template_arg, parent, kind)


register_control("openathan.stop", 0)
register_control("openathan.skip_next", 1)
register_control("openathan.cancel_skip", 2)

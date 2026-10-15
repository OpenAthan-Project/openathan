from esphome import codegen as cg, config_validation as cv
from esphome.components import display, openathan
from esphome.const import CONF_ID

DEPENDENCIES = ["openathan", "display", "wifi"]
ns = cg.esphome_ns.namespace("openathan_display")
StatusDisplay = ns.class_("StatusDisplay", cg.PollingComponent)


def validate_device_id(value):
    from esphome.components import openathan_device
    return cv.use_id(openathan_device.Device)(value)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(StatusDisplay),
    cv.Required("openathan_id"): cv.use_id(openathan.OpenAthan),
    cv.Required("display_id"): cv.use_id(display.Display),
    cv.Optional("layout", default="square_128"): cv.one_of("square_128", "round_360"),
    cv.Optional("stop_button", default=True): cv.boolean,
    cv.Optional("device_id"): validate_device_id,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_openathan(await cg.get_variable(config["openathan_id"])))
    cg.add(var.set_display(await cg.get_variable(config["display_id"])))
    cg.add(var.set_round(config["layout"] == "round_360"))
    cg.add(var.set_stop_button(config["stop_button"]))
    if "device_id" in config:
        device = await cg.get_variable(config["device_id"])
        cg.add(var.set_connection_reader(cg.RawExpression(
            f"[=]() {{ return ::openathan::screen::ConnectionInfo{{{device}->hostname(), "
            f"{device}->ipv4_address(), {device}->connected(), {device}->playback_busy(), "
            f"{device}->phone_setup_active(), {device}->phone_setup_connecting(), {device}->phone_setup_error(), "
            f"{device}->phone_setup_ssid(), {device}->phone_setup_ip()}}; }}")))

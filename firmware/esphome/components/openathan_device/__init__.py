"""Official-device networking and embedded UI; optional to the reusable core."""
import gzip
import json
from pathlib import Path
from importlib.resources import files

from esphome import codegen as cg, config_validation as cv, final_validate as fv
from esphome.components import esp32, openathan, time, wifi
from esphome.const import CONF_ID
from aioesphomeapi.posix_tz import parse_posix_tz
import tzdata

DEPENDENCIES = ["esp32", "wifi", "openathan"]
AUTO_LOAD = ["json", "mdns"]
ns = cg.esphome_ns.namespace("openathan_device")
Device = ns.class_("Device", cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Device),
    cv.Required("openathan_id"): cv.use_id(openathan.OpenAthan),
}).extend(cv.COMPONENT_SCHEMA)


def validate(config):
    full = fv.full_config.get()
    if "improv_serial" not in full or "device_id" not in full["improv_serial"]:
        raise cv.Invalid("OpenAthan requires its external improv_serial USB dispatcher")
    if full["wifi"].get("networks") or "ap" in full["wifi"] or full["wifi"]["enable_on_boot"]:
        raise cv.Invalid("OpenAthan owns Wi-Fi credentials; omit networks/AP and set enable_on_boot false")
    if "web_server" in full or "captive_portal" in full or "esp32_improv" in full:
        raise cv.Invalid("OpenAthan owns the local server and USB-only provisioning")
    return config


FINAL_VALIDATE_SCHEMA = validate


def timezone_entries():
    if tzdata.__version__ != "2026.4":
        raise cv.Invalid("OpenAthan timezone catalog requires tzdata 2026.4")
    names = sorted(set(files("tzdata").joinpath("zones").read_text().splitlines()) | {"UTC"})
    entries = []
    for name in names:
        parsed = parse_posix_tz(time.validate_tz(name))
        def rule(value):
            return {"type": int(value.type), "time_seconds": value.time_seconds, "day": value.day,
                    "month": value.month, "week": value.week, "day_of_week": value.day_of_week}
        rules = {"standard_offset": parsed.std_offset_seconds,
                 "daylight_offset": parsed.dst_offset_seconds if parsed.dst_start.type else 0,
                 "start": rule(parsed.dst_start), "end": rule(parsed.dst_end)}
        entries.append((name, json.dumps(rules, separators=(",", ":"))))
    return entries


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_openathan(await cg.get_variable(config["openathan_id"])))
    wifi.request_wifi_scan_results_listener()
    wifi.request_wifi_connect_state_listener()
    esp32.include_builtin_idf_component("esp_http_server")
    esp32.add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 3072)
    esp32.add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_URI_LEN", 256)
    esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_MD5_C", True)
    entries = timezone_entries()
    table = ",\n".join("{"+json.dumps(name)+","+json.dumps(rules)+"}" for name, rules in entries)
    cg.add_global(cg.RawStatement("static const esphome::openathan_device::ZoneEntry OA_ZONES[] = {\n"+table+"\n};"))
    cg.add(var.set_zones(cg.RawExpression("OA_ZONES"), len(entries)))
    root = Path(__file__).resolve().parents[4] / "web/device-ui"
    for index, (name, mime) in enumerate((("index.html", "text/html; charset=utf-8"),
                                         ("app.js", "text/javascript; charset=utf-8"),
                                         ("style.css", "text/css; charset=utf-8"))):
        data = gzip.compress((root / name).read_bytes(), mtime=0)
        symbol = f"OA_ASSET_{index}"
        cg.add_global(cg.RawStatement(f"static const uint8_t {symbol}[] = {{"+",".join(map(str,data))+"};"))
        cg.add(var.set_asset(index, cg.RawExpression(symbol), len(data), mime))

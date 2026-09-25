"""Compile the production settings bridge with the pinned ESPHome timezone code."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "Run with the pinned ESPHome environment")
class SettingsAdapterTests(unittest.TestCase):
    def test_production_bridge(self):
        import esphome
        package = Path(esphome.__file__).resolve().parent
        core = Path(os.environ.get("OPENATHAN_CORE_LIB", ROOT / "build/libopenathan_core.a"))
        self.assertTrue(core.is_file(), "Build openathan_core before running tests")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "settings-adapter"
            extra = []
            if include := os.environ.get("OPENATHAN_ARDUINOJSON_INCLUDE"):
                shim = Path(directory) / "esphome/components/json/json_util.h"
                shim.parent.mkdir(parents=True)
                shim.write_text('''#pragma once
#define ARDUINOJSON_ENABLE_STD_STRING 1
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <functional>
namespace esphome::json {
inline bool parse_json(const std::string &text, const std::function<bool(JsonObject)> &callback) {
  JsonDocument document;
  return !deserializeJson(document, text) && callback(document.as<JsonObject>());
}
}
''')
                extra = ["-DOPENATHAN_JSON_TEST", "-I" + directory, "-I" + include,
                    str(ROOT / "firmware/esphome/components/openathan/settings_json.cpp")]
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
                "-fno-sanitize-recover=all", "-DUSE_TIME_TIMEZONE", *extra,
                "-I" + str(ROOT / "tests/stubs"), "-I" + str(ROOT / "lib/openathan-core/include"),
                "-I" + str(ROOT / "firmware/esphome/components/openathan"), "-I" + str(package.parent),
                str(ROOT / "tests/settings_adapter_tests.cpp"),
                str(ROOT / "firmware/esphome/components/openathan/openathan.cpp"),
                str(ROOT / "firmware/esphome/components/openathan/nvs_state_store.cpp"),
                str(ROOT / "firmware/esphome/components/openathan/nvs_settings_store.cpp"),
                str(package / "components/time/posix_tz.cpp"), str(core), "-o", str(output)], check=True)
            subprocess.run([str(output)], check=True)

#!/usr/bin/env python3
"""Exercise the firmware's resolved ESP-IDF request formatter and header sender.

The two client functions are compiled unchanged against a host transport shim;
the header and utility implementations are compiled directly from the SDK.
This checks formatting/sending, not TLS, URL parsing or device runtime memory.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:static )?(?:int|esp_err_t) " + name + r"\([^\n]+\)\n\{.*?^\}",
                      source, re.M | re.S)
    if not match:
        raise ValueError(f"Resolved SDK function not found: {name}")
    return match.group(0)


def check(build_dir):
    import yaml
    pins = json.loads((ROOT / "firmware/esphome/feasibility/dependencies.json").read_text())
    lock = yaml.safe_load((build_dir / "dependencies.lock").read_text())["dependencies"]
    if lock["idf"]["version"] != pins["esp_idf"]:
        raise ValueError("Run against the pinned firmware SDK")
    commands = json.loads((build_dir / "build/compile_commands.json").read_text())
    sources = {Path(entry["file"]).resolve() for entry in commands
               if Path(entry["file"]).name == "esp_http_client.c"}
    if len(sources) != 1:
        raise ValueError("Build must resolve exactly one esp_http_client.c")
    client_source = sources.pop()
    source = client_source.read_text()
    functions = "\n\n".join(function(source, name) for name in
                            ("http_client_prepare_first_line", "esp_http_client_request_send"))
    user_agent = re.search(r'^static const char \*DEFAULT_HTTP_USER_AGENT = ("[^"\n]+");$', source, re.M)
    if not user_agent:
        raise ValueError("Resolved SDK default User-Agent not found")
    library = client_source.parent / "lib"
    with tempfile.TemporaryDirectory(prefix="openathan-idf-http-") as temporary:
        output = Path(temporary)
        (output / "esp_err.h").write_text("""#pragma once
typedef int esp_err_t;
enum {ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=1, ESP_ERR_INVALID_ARG=2, ESP_ERR_NOT_FOUND=3};
""")
        (output / "esp_log.h").write_text("""#pragma once
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
""")
        (output / "esp_check.h").write_text("""#pragma once
#include "esp_err.h"
#define unlikely(x) (x)
#define ESP_RETURN_ON_FALSE(a,err,...) do {if (!(a)) return err;} while (0)
""")
        (output / "idf_request.inc").write_text(functions)
        (output / "idf_defaults.h").write_text(f"constexpr const char *USER_AGENT={user_agent[1]};\n")
        flags = ["-fsanitize=undefined", "-fno-sanitize-recover=all",
                 "-I" + str(output), "-I" + str(library / "include")]
        objects = []
        for name in ("http_header", "http_utils"):
            obj = output / f"{name}.o"
            subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-D_GNU_SOURCE", *flags,
                            "-c", str(library / f"{name}.c"), "-o", str(obj)], check=True)
            objects.append(str(obj))
        executable = output / "request-test"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", *flags,
                        str(ROOT / "tests/upgrade_http_tests.cpp"), *objects,
                        "-o", str(executable)], check=True)
        subprocess.run([executable], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    check(args.build_dir)

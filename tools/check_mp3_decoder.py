"""Exercise the production MP3 decoder with its resolved pinned codec on the host.

Generated MP3s are synthetic host fixtures. Never install or play them on hardware.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile

import yaml

ROOT = Path(__file__).resolve().parents[1]


def check(build_dir, source_root=ROOT):
    lock = yaml.safe_load((build_dir / "dependencies.lock").read_text())
    assert lock["dependencies"]["esphome/micro-mp3"]["version"] == "0.4.0", "Unexpected MP3 codec pin"
    codec = build_dir / "managed_components/esphome__micro-mp3"
    assert yaml.safe_load((codec / "idf_component.yml").read_text())["version"] == "0.4.0"
    audio = source_root / "firmware/esphome/components/audio"
    with tempfile.TemporaryDirectory(prefix="openathan-mp3-") as directory:
        work = Path(directory)
        # Use the codec's own host target and source list. Firmware resolves this
        # exact package; do not download a second copy or fake codec results.
        cmake = ["cmake", "-S", str(codec), "-B", str(work / "codec"), "-DCMAKE_BUILD_TYPE=Release"]
        if platform.system() == "Darwin":
            cmake.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0")
        subprocess.run(cmake, check=True, capture_output=True, text=True, timeout=60)
        subprocess.run(["cmake", "--build", str(work / "codec"), "--parallel", "2"],
                       check=True, capture_output=True, text=True, timeout=120)
        clips = []
        for name, length in (("short", ["-t", "0.08"]), ("full", ["-t", "52"]),
                             ("single-frame", ["-frames:a", "1"])):
            path = work / (name + ".mp3")
            subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i",
                            "anullsrc=r=44100:cl=stereo", *length,
                            "-c:a", "libmp3lame", "-b:a", "128k", "-write_xing", "0",
                            "-id3v2_version", "0", str(path)], check=True, timeout=30)
            clips.append(path)
        for product in (False, True):
            binary = work / ("product" if product else "upstream")
            command = [os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
                       "-fno-sanitize-recover=all", "-DUSE_ESP32", "-DUSE_AUDIO_MP3_SUPPORT",
                       "-I" + str(ROOT / "tests/decoder_stubs"), "-I" + str(audio),
                       "-I" + str(codec / "include")]
            if product:
                command.append("-DOPENATHAN_QURAN")
            if platform.system() == "Darwin":
                command.append("-mmacosx-version-min=11.0")
            command.extend([str(ROOT / "tests/mp3_decoder_tests.cpp"),
                            str(audio / "audio_decoder.cpp"), str(audio / "audio_transfer_buffer.cpp"),
                            str(audio / "audio.cpp"), str(work / "codec/libmicro_mp3.a"),
                            "-o", str(binary)])
            subprocess.run(command, check=True, timeout=60)
            subprocess.run([str(binary), *map(str, clips)], check=True, timeout=30)
            print(json.dumps({"quran_enabled": product, "resolved_micro_mp3": "0.4.0", "passed": True}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="Production source tree; permits a before-fix reproduction with the same host fixture")
    args = parser.parse_args()
    check(args.build_dir, args.source_root)

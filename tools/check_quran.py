"""Exercise the production Quran worker against deterministic HTTP/task adapters."""
import os
from pathlib import Path
import subprocess
import tempfile
import argparse

ROOT = Path(__file__).resolve().parents[1]

def check(arduinojson):
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / "quran-tests"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
            "-fno-sanitize-recover=all", "-I" + str(ROOT / "tests/upgrade_stubs"),
            "-I" + str(ROOT / "tests/quran_stubs"),
            "-I" + str(ROOT / "tests/stubs"), "-I" + arduinojson,
            "-I" + str(ROOT / "lib/openathan-core/include"),
            "-I" + str(ROOT / "firmware/esphome/components/openathan_device"),
            str(ROOT / "tests/quran_service_tests.cpp"),
            str(ROOT / "firmware/esphome/components/openathan_device/quran.cpp"),
            "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        # Separate translation units retain each production adapter's host environment.
        worker = Path(directory) / "quran.o"
        common = [os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
            "-fno-sanitize-recover=all", "-I" + str(ROOT / "tests/pyramid_quran_stubs"),
            "-I" + str(ROOT / "tests/quran_stubs"), "-I" + arduinojson,
            "-I" + str(ROOT / "lib/openathan-core/include"),
            "-I" + str(ROOT / "firmware/esphome/components/openathan_device")]
        subprocess.run(common + ["-I" + str(ROOT / "tests/upgrade_stubs"),
            "-I" + str(ROOT / "tests/stubs"), "-c",
            str(ROOT / "firmware/esphome/components/openathan_device/quran.cpp"),
            "-o", str(worker)], check=True)
        worker_controls = Path(directory) / "worker-controls.o"
        subprocess.run(common + ["-I" + str(ROOT / "tests/upgrade_stubs"),
            "-I" + str(ROOT / "tests/stubs"), "-c",
            str(ROOT / "tests/pyramid_quran_worker.cpp"), "-o", str(worker_controls)], check=True)
        speaker = ROOT / "third_party/esphome-i2s-audio/components/i2s_audio/speaker"
        integration = Path(directory) / "quran-volume-tests"
        subprocess.run(common + ["-DUSE_ESP32", "-DUSE_OPENATHAN_VOLUME_FENCE",
            "-DOPENATHAN_QURAN_VOLUME_INTEGRATION", "-I" + str(ROOT / "tests/i2s_stubs"),
            "-I" + str(ROOT / "tests/pyramid_stubs"), "-I" + str(ROOT / "tests/waveshare_stubs"),
            "-I" + str(ROOT / "tests/stubs"),
            "-I" + str(ROOT / "third_party/esphome-i2s-audio"),
            "-I" + str(ROOT / "firmware"),
            "-I" + str(speaker), "-I" + str(ROOT / "third_party/adhan-cpp/include"),
            str(ROOT / "tests/pyramid_volume_tests.cpp"),
            str(speaker / "i2s_audio_speaker.cpp"), str(speaker / "i2s_audio_speaker_standard.cpp"),
            str(ROOT / "firmware/esphome/components/voice_pyramid/voice_pyramid.cpp"),
            str(ROOT / "firmware/esphome/components/openathan/nvs_settings_store.cpp"),
            *[str(p) for p in sorted((ROOT / "lib/openathan-core/src").glob("*.cpp"))],
            *[str(p) for p in sorted((ROOT / "third_party/adhan-cpp/src").glob("*.cpp"))],
            str(worker), str(worker_controls), "-o", str(integration)], check=True)
        subprocess.run([str(integration)], check=True)
        print("Atom volume/Quran continuity, pending admission, zero, rapid changes, Stop and faults passed")
        qualified = Path(directory) / "quran-qualification-tests"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fsanitize=undefined",
            "-fno-sanitize-recover=all", "-DOPENATHAN_QURAN", "-DOPENATHAN_QURAN_QUALIFICATION",
            "-DOPENATHAN_PROVISIONING_TEST_STORAGE", "-DOPENATHAN_UPDATES_ENABLED=0",
            '-DOPENATHAN_QURAN_QUALIFICATION_ORIGIN="https://192.168.1.2:8443"',
            '-DOPENATHAN_QURAN_QUALIFICATION_CA="host test CA"',
            "-I" + str(ROOT / "tests/upgrade_stubs"), "-I" + str(ROOT / "tests/stubs"),
            "-I" + arduinojson, "-I" + str(ROOT / "firmware"),
            "-I" + str(ROOT / "firmware/esphome/components/openathan_device"),
            "-I" + str(ROOT / "lib/openathan-core/include"),
            str(ROOT / "tests/quran_qualification_tests.cpp"),
            str(ROOT / "firmware/esphome/components/openathan_quran_qualification/qualification.cpp"),
            "-o", str(qualified)], check=True)
        subprocess.run([str(qualified)], check=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduinojson", required=True)
    check(parser.parse_args().arduinojson)

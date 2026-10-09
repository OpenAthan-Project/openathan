"""In-memory non-executable firmware/audio fixtures. Never upload or flash."""
import hashlib
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from release_artifacts import PARTITIONS


def esp_image(payload=b"non-executable test bytes" * 4, flash_bytes=0x800000):
    payload += b"\0" * (-len(payload) % 4)
    header = bytearray(24)
    header[0:4] = bytes((0xe9, 1, 0, 0x30 if flash_bytes == 0x800000 else 0x40))
    struct.pack_into("<H", header, 12, 9)
    header[23] = 1
    result = header + struct.pack("<II", 0x3fc88000, len(payload)) + payload
    result += b"\0" * ((15 - len(result)) % 16)
    checksum = 0xef
    for byte in payload:
        checksum ^= byte
    result += bytes((checksum,))
    result += hashlib.sha256(result).digest()
    return bytes(result)


def firmware(flash_bytes=0x800000):
    app = esp_image(flash_bytes=flash_bytes)
    factory = bytearray(b"\xff" * 0x10000 + app)
    boot = esp_image(b"non-executable boot fixture", flash_bytes=flash_bytes)
    factory[:len(boot)] = boot
    for index, (name, kind, subtype, offset, size) in enumerate(PARTITIONS):
        struct.pack_into("<HBBII16sI", factory, 0x8000 + index * 32,
                         0x50aa, kind, subtype, offset, size, name.encode(), 0)
    end = 0x8000 + len(PARTITIONS) * 32
    factory[end:end + 32] = b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(factory[0x8000:end]).digest()
    return bytes(factory), app


def compiled(directory, pins, partitions):
    (directory / "src/esphome/core").mkdir(parents=True)
    (directory / "src/esphome/core/defines.h").write_text("")
    import yaml
    factory, app = firmware()
    (directory / "build").mkdir(parents=True)
    (directory / "build/firmware.factory.bin").write_bytes(factory)
    (directory / "build/firmware.ota.bin").write_bytes(app)
    (directory / "build/compile_commands.json").write_text(pins["xtensa_esp_elf"])
    (directory / "partitions.csv").write_bytes(partitions)
    lock = {name: {**entry, "source": {"type": "service"}}
            for name, entry in {**pins["managed_components"], **pins["optional_managed_components"]}.items()}
    lock["idf"] = {"version": pins["esp_idf"], "source": {"type": "idf"}}
    (directory / "dependencies.lock").write_text(yaml.safe_dump({"dependencies": lock}))


def registry(payloads):
    return dict(schema=1, tracks=[dict(role=role, sha256=hashlib.sha256(payload).hexdigest(),
                                     source="https://example.org/test", license="https://example.org/test-license",
                                     attribution="Synthetic test-only fixture", approved=True)
                                 for role, payload in zip(("normal", "fajr"), payloads)])

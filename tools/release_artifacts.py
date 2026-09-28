"""Validation shared by release preparation and upload. No network or device I/O."""
import hashlib
import io
import json
import re
import struct
from urllib.parse import urlparse

from audio_image import PARTITION_OFFSET, PARTITION_SIZE, build_image

REPOSITORY = "OpenAthan-Project/openathan"
HARDWARE = "atoms3r-c126-pyramid-a167"
LAYOUT = "dual-2m-audio-3_5m-v1"
CONFIGURATION = "firmware/esphome/openathan.yaml"
MEDIA_REGISTRY = "release/recordings.json"
LICENSE_PATH = "AUDIO-LICENSES.md"
PINS_PATH = "firmware/esphome/feasibility/dependencies.json"
ASSETS = ("manifest.json", "firmware.factory.bin", "athan-audio.bin", "SHA256SUMS", "build-report.json")
SHA = re.compile(r"[a-f0-9]{40}\Z")
TAG = re.compile(r"v[0-9]+\.[0-9]+\.[0-9]+\Z")
PARTITIONS = (("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xe000, 0x2000),
              ("app0", 0, 0x10, 0x10000, 0x200000), ("app1", 0, 0x11, 0x210000, 0x200000),
              ("athan_audio", 1, 0x40, PARTITION_OFFSET, PARTITION_SIZE))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n").encode()


def read_json(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"Duplicate metadata key: {key}")
            result[key] = value
        return result
    def reject_constant(value):
        raise ValueError("Non-finite metadata")
    return json.loads(data, object_pairs_hook=unique, parse_constant=reject_constant)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_file(path, maximum):
    require(not path.is_symlink() and path.is_file(), f"Expected regular file: {path.name}")
    with path.open("rb") as handle:
        data = handle.read(maximum + 1)
    require(0 < len(data) <= maximum, f"Invalid file size: {path.name}")
    return data


def esp_image(data, exact=False):
    # esptool parses segment lengths, but callers must explicitly check its
    # checksum/digest results; parsing alone does not establish integrity.
    from esptool import FatalError
    from esptool.bin_image import ESP32S3FirmwareImage
    require(len(data) >= 24 and data[0] == 0xe9 and data[1] in range(1, 17), "Invalid ESP image header")
    require(struct.unpack_from("<H", data, 12)[0] == 9, "Image is not for ESP32-S3")
    require(data[3] >> 4 == 3, "Image does not declare 8 MiB flash")
    try:
        image = ESP32S3FirmwareImage(io.BytesIO(data))
        require(image.checksum == image.calculate_checksum(), "ESP image checksum mismatch")
        require(image.append_digest and image.stored_digest == image.calc_digest, "ESP image digest mismatch")
        length = image.data_length + 32
        require(not exact or length == len(data), "Unexpected bytes after ESP application")
        return length
    except (FatalError, IndexError, TypeError, struct.error, EOFError) as error:
        raise ValueError("Truncated ESP image") from error


def validate_firmware_images(factory, app=None):
    require(0x10020 <= len(factory) < PARTITION_OFFSET, "Invalid factory image size")
    esp_image(factory[:0x8000])
    table = factory[0x8000:0x9000]
    for index, (name, kind, subtype, offset, size) in enumerate(PARTITIONS):
        record = table[index * 32:(index + 1) * 32]
        expected = struct.pack("<HBBII16sI", 0x50aa, kind, subtype, offset, size, name.encode(), 0)
        require(record == expected, "Factory partition table differs from supported layout")
    end = len(PARTITIONS) * 32
    require(table[end:end + 16] == b"\xeb\xeb" + b"\xff" * 14, "Missing partition checksum")
    require(table[end + 16:end + 32] == hashlib.md5(table[:end]).digest(), "Partition checksum mismatch")
    require(table[end + 32:] == b"\xff" * (4096 - end - 32), "Unexpected extra factory partition")
    require(factory[0x9000:0xe000] == b"\xff" * 0x5000, "Factory image contains initialized NVS")
    app_length = esp_image(factory[0x10000:])
    require(app_length <= 0x180000, "Application exceeds the 1.5 MiB growth-budget target")
    actual = factory[0x10000:0x10000 + app_length]
    if app is not None:
        require(actual == app, "Factory and OTA application payloads differ")
        esp_image(app, exact=True)
    require(factory[0x10000 + app_length:] == b"\xff" * (len(factory) - 0x10000 - app_length),
            "Unexpected data after factory application")
    return actual


def approved_tracks(registry, license_bytes, payloads):
    require(isinstance(registry, dict) and registry.get("schema") == 1, "Unsupported recording registry")
    entries = registry.get("tracks")
    require(isinstance(entries, list) and len(entries) == 2, "Both approved recordings are required")
    require(license_bytes.strip(), "Committed audio attribution is missing")
    for role, entry, payload in zip(("normal", "fajr"), entries, payloads):
        require(isinstance(entry, dict) and entry.get("role") == role and entry.get("approved") is True,
                f"The {role} recording has not been approved")
        require(entry.get("sha256") == digest(payload), f"The {role} recording does not match its approved hash")
        for field in ("source", "license"):
            value = entry.get(field)
            require(isinstance(value, str) and urlparse(value).scheme == "https" and urlparse(value).netloc,
                    f"Missing {role} {field} URL")
        require(isinstance(entry.get("attribution"), str) and entry["attribution"].strip(),
                f"Missing {role} attribution")
        require(entry["sha256"].encode() in license_bytes, f"Audio license document must identify the {role} hash")
    return entries


def audio_payloads(media):
    require(len(media) == PARTITION_SIZE and media[:8] == b"OAUDIO01", "Invalid audio partition")
    require(struct.unpack_from("<I", media, 12)[0] == 2 and hashlib.sha256(media[:112]).digest() == media[112:144],
            "Invalid audio metadata")
    payloads = []
    for index in range(2):
        track, offset, length, codec, sha = struct.unpack_from("<IIII32s", media, 16 + index * 48)
        require(track == index + 1 and codec == 1 and length > 0 and offset >= 4096 and
                offset + length <= len(media), "Invalid audio track bounds")
        payload = media[offset:offset + length]
        require(hashlib.sha256(payload).digest() == sha, "Audio track hash mismatch")
        payloads.append(payload)
    # Canonical reconstruction also checks overlap, alignment, used length and padding.
    require(build_image(*payloads)[0] == media, "Noncanonical audio partition")
    return payloads


def make_manifest(commit, tag, factory, audio):
    require(isinstance(commit, str) and SHA.fullmatch(commit), "An exact source commit is required")
    require(isinstance(tag, str) and TAG.fullmatch(tag), "Version must have the form vX.Y.Z")
    return dict(schema=1, repository=REPOSITORY, tag=tag, commit=commit, hardware=HARDWARE,
                chip="ESP32-S3", flashBytes=0x800000, layout=LAYOUT, provisioningProtocol=1,
                media=dict(redistributionApproved=True,
                           licenseUrl=f"https://github.com/{REPOSITORY}/blob/{commit}/{LICENSE_PATH}"),
                parts=[dict(role=role, file=name, offset=offset, bytes=len(data), sha256=digest(data))
                       for role, name, offset, data in (("factory", ASSETS[1], 0, factory),
                                                       ("audio", ASSETS[2], PARTITION_OFFSET, audio))])


def checksums(files):
    return "".join(f"{digest(files[name])}  {name}\n" for name in sorted(files) if name != "SHA256SUMS").encode()


def validate_bundle(directory):
    require(directory.is_dir() and not directory.is_symlink(), "Bundle must be a regular directory")
    require({p.name for p in directory.iterdir()} == set(ASSETS), "Bundle must contain exactly the five release assets")
    limits = (16384, PARTITION_OFFSET - 1, PARTITION_SIZE, 1024, 65536)
    files = {name: read_file(directory / name, limit) for name, limit in zip(ASSETS, limits)}
    manifest = read_json(files["manifest.json"])
    require(isinstance(manifest, dict), "Invalid release manifest")
    expected = make_manifest(manifest.get("commit"), manifest.get("tag"), files[ASSETS[1]], files[ASSETS[2]])
    require(json_bytes(manifest) == json_bytes(expected),
            "Release manifest does not match the supported contract and files")
    app = validate_firmware_images(files[ASSETS[1]])
    payloads = audio_payloads(files[ASSETS[2]])
    require(all(payload[:4096] not in app for payload in payloads), "Recording data found in application")
    require(files["SHA256SUMS"] == checksums(files), "Bundle checksums differ")
    report = read_json(files["build-report.json"])
    require(isinstance(report, dict) and report.get("commit") == manifest["commit"] and
            report.get("configuration") == CONFIGURATION and report.get("schema") == 1,
            "Invalid build report identity")
    expected_metrics = dict(application_bytes=len(app), application_sha256=digest(app),
                            application_slot_bytes=0x200000, slot_free_bytes=0x200000-len(app),
                            application_budget_remaining_bytes=0x180000-len(app), growth_target_passed=True)
    require(isinstance(report.get("capacity"), dict), "Invalid capacity report")
    require(all(report["capacity"].get(key) == value for key, value in expected_metrics.items()),
            "Build report does not match the application")
    return manifest, report, files, payloads

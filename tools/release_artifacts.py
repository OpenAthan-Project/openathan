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
WAVESHARE = "waveshare-esp32-s3-touch-lcd-1_85c-box-v2"
AUDIO_BOARD = "waveshare-esp32-s3-audio-board"
PROFILES = {
    HARDWARE: dict(configuration=CONFIGURATION, flash_bytes=0x800000, prefix=""),
    WAVESHARE: dict(configuration="firmware/esphome/waveshare/production.yaml",
                    flash_bytes=0x1000000, prefix="waveshare-box-v2."),
    AUDIO_BOARD: dict(configuration="firmware/esphome/waveshare-audio-board/production.yaml",
                      flash_bytes=0x1000000, prefix="waveshare-audio-board."),
}


def profile(hardware):
    require(hardware in PROFILES, "Unsupported release hardware")
    return PROFILES[hardware]


def asset_name(hardware, name):
    return name if name == "athan-audio.bin" else profile(hardware)["prefix"] + name


def configuration_hardware(configuration):
    for hardware, entry in PROFILES.items():
        if entry["configuration"] == configuration:
            return hardware
    raise ValueError("Unsupported production configuration")
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


def esp_image(data, exact=False, *, flash_bytes=0x800000):
    # esptool parses segment lengths, but callers must explicitly check its
    # checksum/digest results; parsing alone does not establish integrity.
    from esptool import FatalError
    from esptool.bin_image import ESP32S3FirmwareImage
    require(len(data) >= 24 and data[0] == 0xe9 and data[1] in range(1, 17), "Invalid ESP image header")
    require(struct.unpack_from("<H", data, 12)[0] == 9, "Image is not for ESP32-S3")
    require(flash_bytes in (0x800000, 0x1000000), "Unsupported flash capacity")
    require(data[3] >> 4 == (3 if flash_bytes == 0x800000 else 4),
            f"Image does not declare {flash_bytes // 0x100000} MiB flash")
    try:
        image = ESP32S3FirmwareImage(io.BytesIO(data))
        require(image.checksum == image.calculate_checksum(), "ESP image checksum mismatch")
        require(image.append_digest and image.stored_digest == image.calc_digest, "ESP image digest mismatch")
        length = image.data_length + 32
        require(not exact or length == len(data), "Unexpected bytes after ESP application")
        return length
    except (FatalError, IndexError, TypeError, struct.error, EOFError) as error:
        raise ValueError("Truncated ESP image") from error


QUALIFICATION_MARKERS = (b"OPENATHAN_QUALIFICATION_V1", b"OPENATHAN_QURAN_QUALIFICATION_V1", b"OPENATHAN_DUA_QUALIFICATION_V1", b"before_boot_selection\0",
                         b"after_boot_selection\0", b"Qualification baseline", b"worker_stack_min_free\0")
ISOLATED_STORAGE_MARKERS = (b"oa_test\0", b"oa_setup_test\0", b"oa_network_test\0", b"oa_upgrade_test\0")


def reject_test_material(app, *, allow_qualification=False, allow_isolated=False):
    require(allow_qualification or not any(marker in app for marker in QUALIFICATION_MARKERS),
            "Qualification firmware cannot enter production release bundles")
    require(allow_isolated or not any(marker in app for marker in ISOLATED_STORAGE_MARKERS),
            "Isolated test firmware cannot enter production release bundles")


def validate_firmware_images(factory, app=None, *, allow_qualification=False, allow_isolated=False, flash_bytes=0x800000, hardware=HARDWARE):
    require(0x10020 <= len(factory) < PARTITION_OFFSET, "Invalid factory image size")
    require(flash_bytes == profile(hardware)["flash_bytes"], "Hardware and image flash capacity differ")
    esp_image(factory[:0x8000], flash_bytes=flash_bytes)
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
    app_length = esp_image(factory[0x10000:], flash_bytes=flash_bytes)
    require(app_length <= 0x180000, "Application exceeds the 1.5 MiB growth-budget target")
    actual = factory[0x10000:0x10000 + app_length]
    reject_test_material(actual, allow_qualification=allow_qualification, allow_isolated=allow_isolated)
    if app is not None:
        require(actual == app, "Factory and OTA application payloads differ")
        esp_image(app, exact=True, flash_bytes=flash_bytes)
    require(factory[0x10000 + app_length:] == b"\xff" * (len(factory) - 0x10000 - app_length),
            "Unexpected data after factory application")
    return actual


def approved_tracks(registry, license_bytes, payloads):
    require(isinstance(registry, dict) and registry.get("schema") == 1, "Unsupported recording registry")
    entries = registry.get("tracks")
    require(isinstance(entries, list) and len(entries) in (2, 3) and
            len(payloads) in (2, 3) and len(entries) >= len(payloads), "All bundled recordings require approval")
    # Approval permits optional dua packaging without requiring its inclusion.
    entries = entries[:len(payloads)]
    require(license_bytes.strip(), "Committed audio attribution is missing")
    for role, entry, payload in zip(("normal", "fajr", "dua"), entries, payloads):
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


def audio_payloads(media, public_pem=None):
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
    from dua_media import BANK_START, BANK_SIZE, read_bank
    if struct.unpack_from("<I", media, 8)[0] <= BANK_START and media[BANK_START:] != b"\xff" * (PARTITION_SIZE - BANK_START):
        require(public_pem is not None, "Dua banks require the source-bound production trust key")
        dua, envelope, generation = read_bank(media[BANK_START:BANK_START + BANK_SIZE], public_pem)
        require(generation == 1 and media[BANK_START + BANK_SIZE:] == b"\xff" * BANK_SIZE,
                "Fresh-install dua must use only bank zero at generation one")
        require(build_image(*payloads, dua=dua, dua_envelope=envelope)[0] == media, "Noncanonical audio partition")
        payloads.append(dua)
    else:
        require(build_image(*payloads)[0] == media, "Noncanonical audio partition")
    return payloads


def phone_setup_version(metadata):
    require(isinstance(metadata, dict), "Invalid phone setup metadata")
    if "phoneSetup" not in metadata:
        return None
    value = metadata["phoneSetup"]
    require(type(value) is int and value == 1, "Unsupported phoneSetup capability")
    return value


def make_manifest(commit, tag, factory, audio, hardware=HARDWARE, *, phone_setup=None):
    require(isinstance(commit, str) and SHA.fullmatch(commit), "An exact source commit is required")
    require(isinstance(tag, str) and TAG.fullmatch(tag), "Version must have the form vX.Y.Z")
    manifest = dict(schema=1, repository=REPOSITORY, tag=tag, commit=commit, hardware=hardware,
                chip="ESP32-S3", flashBytes=profile(hardware)["flash_bytes"], layout=LAYOUT, provisioningProtocol=1,
                media=dict(redistributionApproved=True,
                           licenseUrl=f"https://github.com/{REPOSITORY}/blob/{commit}/{LICENSE_PATH}"),
                parts=[dict(role=role, file=asset_name(hardware, name), offset=offset, bytes=len(data), sha256=digest(data))
                       for role, name, offset, data in (("factory", ASSETS[1], 0, factory),
                                                       ("audio", ASSETS[2], PARTITION_OFFSET, audio))])
    if phone_setup is not None:
        manifest["phoneSetup"] = phone_setup_version({"phoneSetup": phone_setup})
    return manifest


def checksums(files):
    return "".join(f"{digest(files[name])}  {name}\n" for name in sorted(files) if name != "SHA256SUMS").encode()


def validate_bundle(directory):
    require(directory.is_dir() and not directory.is_symlink(), "Bundle must be a regular directory")
    from upgrade_artifacts import UPGRADE_ASSETS
    names = {p.name for p in directory.iterdir()}
    groups = tuple(board for board in PROFILES if asset_name(board, "manifest.json") in names)
    require(bool(groups), "Bundle has no supported hardware manifest")
    dua_assets = {"dua.json", "dua.mp3"}
    has_dua = bool(names & dua_assets)
    allowed = set(dua_assets) if has_dua else set()
    for board in groups:
        allowed.update(asset_name(board, name) for name in ASSETS)
        if asset_name(board, "upgrade.json") in names:
            allowed.update(asset_name(board, name) for name in UPGRADE_ASSETS)
    require(names == allowed, "Bundle contains unexpected or incomplete release assets")
    limits = dict(zip(ASSETS, (16384, PARTITION_OFFSET - 1, PARTITION_SIZE, 1024, 65536)))
    limits.update({"firmware.ota.bin": 1572864, "upgrade.json": 8192})
    files = {asset_name(board, name): read_file(directory / asset_name(board, name), limit)
             for board in groups for name, limit in limits.items() if asset_name(board, name) in names}
    if has_dua:
        require(dua_assets <= names, "Incomplete dua release assets")
        files["dua.json"] = read_file(directory / "dua.json", 2048)
        files["dua.mp3"] = read_file(directory / "dua.mp3", 258048)
    results = [validate_profile_files(files, board) for board in groups]
    require(all(result[0]["commit"] == results[0][0]["commit"] and
                result[0]["tag"] == results[0][0]["tag"] for result in results),
            "Hardware bundles have different source/version")
    manifest, report, payloads = results[0]
    return manifest, report, files, payloads


def validate_profile_files(all_files, hardware):
    def data(name):
        return all_files[asset_name(hardware, name)]
    manifest = read_json(data("manifest.json"))
    require(isinstance(manifest, dict), "Invalid release manifest")
    expected = make_manifest(manifest.get("commit"), manifest.get("tag"),
                             data("firmware.factory.bin"), data("athan-audio.bin"), hardware,
                             phone_setup=phone_setup_version(manifest))
    require(json_bytes(manifest) == json_bytes(expected), "Release manifest does not match the supported contract and files")
    app = validate_firmware_images(data("firmware.factory.bin"),
                                  flash_bytes=profile(hardware)["flash_bytes"], hardware=hardware)
    if asset_name(hardware, "firmware.ota.bin") in all_files:
        require(data("firmware.ota.bin") == app, "Upgrade application differs from factory image")
    from dua_media import BANK_START, BANK_SIZE, read_bank, validate as validate_dua
    from release import source_file
    has_dua = "dua.json" in all_files
    public_pem = source_file(manifest["commit"], "release/upgrade-public-key.pem") if has_dua else None
    payloads = audio_payloads(data("athan-audio.bin"), public_pem)
    require((len(payloads) == 3) == has_dua, "Dua bank and downloadable assets must be bundled together")
    if has_dua:
        validate_dua(all_files["dua.json"], all_files["dua.mp3"], public_pem, manifest["tag"])
        audio, envelope, _ = read_bank(data("athan-audio.bin")[BANK_START:BANK_START + BANK_SIZE], public_pem)
        require(audio == all_files["dua.mp3"] and envelope == all_files["dua.json"], "Dua assets differ from installed bank")
    require(all(payload[:4096] not in app for payload in payloads), "Recording data found in application")
    board_names = {asset_name(hardware, name) for name in (*ASSETS, "firmware.ota.bin", "upgrade.json")}
    board_files = {name: value for name, value in all_files.items()
                   if name in board_names or name in ("dua.json", "dua.mp3")}
    checksum_name = asset_name(hardware, "SHA256SUMS")
    checksum = "".join(f"{digest(board_files[name])}  {name}\n" for name in sorted(board_files)
                       if name != checksum_name).encode()
    require(data("SHA256SUMS") == checksum, "Bundle checksums differ")
    report = read_json(data("build-report.json"))
    require(isinstance(report, dict) and report.get("commit") == manifest["commit"] and
            report.get("configuration") == profile(hardware)["configuration"] and report.get("schema") == 1,
            "Invalid build report identity")
    expected_metrics = dict(application_bytes=len(app), application_sha256=digest(app),
                            application_slot_bytes=0x200000, slot_free_bytes=0x200000-len(app),
                            application_budget_remaining_bytes=0x180000-len(app), growth_target_passed=True)
    require(isinstance(report.get("capacity"), dict), "Invalid capacity report")
    require(all(report["capacity"].get(key) == value for key, value in expected_metrics.items()),
            "Build report does not match the application")
    return manifest, report, payloads

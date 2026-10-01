#!/usr/bin/env python3
"""Scoped USB inspection and reviewed application/bootloader transitions.

No erase-flash, partition-table, NVS or audio writes; no automatic write retry.
Every apply rechecks fresh identity, security and the complete scoped baseline.
"""
import argparse
from datetime import datetime, timezone
import importlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import zlib

from audio_image import ROOT
from release_artifacts import digest, esp_image, json_bytes, read_file, read_json, require
from upgrade_qualification import MARKER, private_directory, write_private
from upgrade_artifacts import image_version

REGIONS = {"bootloader": (0, 0x8000), "control": (0x8000, 0x8000),
           "app0": (0x10000, 0x200000), "app1": (0x210000, 0x200000),
           "audio": (0x410000, 0x380000)}
PROTECTED = ("openathan", "oa_setup", "oa_network", "oa_validation", "oa_upgrade")


def selection(data):
    require(len(data) == 0x2000, "Invalid OTA metadata size")
    if data == b"\xff" * len(data):
        return dict(slot=0, offset=0x10000, sequence=None, state="erased")
    entries = []
    for offset in (0, 0x1000):
        page = data[offset:offset+0x1000]
        if page == b"\xff" * len(page):
            continue
        sequence = struct.unpack_from("<I", page)[0]
        state, crc = struct.unpack_from("<II", page, 24)
        require(sequence not in (0, 0xffffffff) and
                crc == zlib.crc32(struct.pack("<I", sequence), 0xffffffff), "Malformed OTA selection")
        require(state in (0, 1, 2, 3, 4, 0xffffffff), "Unknown OTA application state")
        if state not in (3, 4):
            entries.append((sequence, state))
    require(entries and len({seq for seq, _ in entries}) == len(entries), "Ambiguous OTA selection")
    sequence, state = max(entries)
    return dict(slot=(sequence-1) % 2, offset=0x10000 + ((sequence-1) % 2)*0x200000,
                sequence=sequence, state={0: "new", 1: "pending", 2: "valid", 0xffffffff: "undefined"}[state])


def idf_modules(idf):
    version = (idf / "version.txt").read_text().strip()
    require(version == "v5.5.5", "Use the pinned ESP-IDF v5.5.5 parser")
    sys.path.insert(0, str(idf / "components/partition_table"))
    sys.path.insert(0, str(idf / "components/nvs_flash/nvs_partition_tool"))
    return importlib.import_module("gen_esp32part"), importlib.import_module("nvs_parser")


def records(nvs, parser):
    partition = parser.NVS_Partition("nvs", bytearray(nvs))
    written = []
    for page in partition.pages:
        if page.is_empty:
            continue
        require(page.header["status"] in ("Active", "Full"), "Unexpected NVS page state")
        require(page.header["crc"]["original"] == page.header["crc"]["computed"], "NVS page CRC mismatch")
        for entry in page.entries:
            if entry.state != "Written":
                continue
            crc = entry.metadata["crc"]
            require(crc["original"] == crc["computed"], "NVS entry CRC mismatch")
            if entry.metadata["type"] in ("blob", "blob_data", "string"):
                require(crc["data_original"] == crc["data_computed"], "NVS payload CRC mismatch")
            written.append(entry)
    namespaces = {}
    for entry in written:
        if entry.metadata["namespace"] == 0:
            identifier = entry.data["value"]
            require(identifier not in namespaces and entry.key not in namespaces.values(), "Ambiguous NVS namespace")
            namespaces[identifier] = entry.key
    result = {name: dict(present=name in namespaces.values(), records={}) for name in PROTECTED}
    for entry in written:
        namespace = namespaces.get(entry.metadata["namespace"])
        if namespace not in PROTECTED:
            continue
        kind = entry.metadata["type"]
        require(kind in ("blob_index", "blob_data", "blob", "string"), "Unexpected production record type")
        if kind == "blob_data":
            continue
        if kind == "blob_index":
            chunks = []
            for index in range(entry.data["chunk_start"], entry.data["chunk_start"] + entry.data["chunk_count"]):
                matches = [other for other in written if other.metadata["type"] == "blob_data" and
                           other.key == entry.key and other.metadata["namespace"] == entry.metadata["namespace"] and
                           other.metadata["chunk_index"] == index]
                require(len(matches) == 1, "Ambiguous NVS blob chunk")
                chunk = matches[0]
                chunks.append(b"".join(bytes(child.raw) for child in chunk.children)[:chunk.data["size"]])
            data = b"".join(chunks)
        else:
            data = b"".join(bytes(child.raw) for child in entry.children)[:entry.data["size"]]
        require(len(data) == entry.data["size"], "NVS payload size mismatch")
        if entry.key in ("settings", "scheduler"):
            size, magic = (192, b"OAC1") if entry.key == "settings" else (36, b"OAS1")
            require(len(data) == size and data[:4] == magic and
                    struct.unpack_from("<I", data, size-4)[0] == zlib.crc32(data[:-4]),
                    "Invalid durable settings or scheduler record")
        values = result[namespace]["records"]
        require(entry.key not in values, "Duplicate production NVS record")
        values[entry.key] = dict(bytes=len(data), sha256=digest(data))
    return result


def analyze(images, idf):
    partitions, parser = idf_modules(idf)
    control = images["control"]
    actual = partitions.PartitionTable.from_binary(control[:0x1000])
    expected = partitions.PartitionTable.from_csv((ROOT / "firmware/esphome/feasibility/partitions.csv").read_text())
    fields = lambda table: [(p.name, p.type, p.subtype, p.offset, p.size, tuple(p.get_flags_list())) for p in table]
    require(fields(actual) == fields(expected), "Device partition table differs from the supported layout")
    selected = selection(control[0x6000:])
    app = images[f"app{selected['slot']}"]
    length = esp_image(app)
    require(length <= 1572864, "Selected application exceeds its budget")
    return dict(selection=selected, application_bytes=length, application_sha256=digest(app[:length]),
                regions={name: digest(data) for name, data in images.items()},
                records=records(control[0x1000:0x6000], parser))


class Device:
    def __init__(self, port, expected_mac):
        import esptool
        self.esp = esptool.get_default_connected_device([port], port, 7, 115200, chip="esp32s3")
        try:
            mac = ":".join(f"{byte:02x}" for byte in self.esp.read_mac())
            require(mac == expected_mac.lower(), "USB device MAC differs from the reviewed identity")
            security = self.esp.get_security_info()
            require(security["flags"] == 0 and security["flash_crypt_cnt"].bit_count() % 2 == 0,
                    "Unexpected security configuration; no flash writes permitted")
            self.esp = self.esp.run_stub()
            # Match the official CLI: attach SPI flash after loading the stub,
            # before identification or reads. Bare ROM flash_id is not reliable.
            from esptool.cmds import attach_flash
            attach_flash(self.esp)
            require((self.esp.flash_id() >> 16) & 0xff == 0x17, "Expected exactly 8 MiB of flash")
        except BaseException:
            self.close(); raise

    def close(self):
        self.esp._port.close()

    def read(self, address, size):
        from esptool.cmds import read_flash
        return read_flash(self.esp, address, size, no_progress=True)

    def write_verified(self, address, data):
        from esptool.cmds import write_flash, verify_flash
        write_flash(self.esp, [(address, data)])
        verify_flash(self.esp, [(address, data)])
        require(self.read(address, len(data)) == data, "Independent flash readback differs")


def capture(device, directory, idf, expected_mac):
    directory = private_directory(directory, fresh=True)
    images = {}
    for name, (address, size) in REGIONS.items():
        first = device.read(address, size)
        second = device.read(address, size)
        require(len(first) == size and first == second, f"Paired {name} reads differ")
        write_private(directory / f"{name}.bin", first)
        images[name] = first
        print(f"Paired {name} read complete", flush=True)
    report = analyze(images, idf)
    report.update(mac=expected_mac.lower(), utc=datetime.now(timezone.utc).isoformat(), full_flash_backup=False)
    app = images[f"app{report['selection']['slot']}"][:report["application_bytes"]]
    write_private(directory / "recovery.ota.bin", app)
    write_private(directory / "snapshot.json", json_bytes(report))
    return report


def make_plan(snapshot, application, bootloader=None):
    directory = private_directory(snapshot)
    report = read_json(read_file(directory / "snapshot.json", 65536))
    require(report["selection"]["state"] in ("valid", "undefined", "erased"), "Current slot must be bootable and not pending")
    app = read_file(application, 1572864)
    esp_image(app, exact=True)
    if bootloader:
        require(MARKER in app and image_version(app) == "v0.0.1", "Bootloader transitions require the healthy v0.0.1 baseline")
        boot = read_file(bootloader, 0x8000)
        allowlist = read_json((ROOT / "release/rollback-bootloaders.json").read_bytes())
        require(len(boot) == 0x8000 and digest(boot) in allowlist["sha256"], "Bootloader is not in the reviewed allowlist")
        esp_image(boot)
    else:
        require(MARKER not in app, "Production restoration requires a production application")
        require(image_version(app) == read_json((ROOT / "release/firmware.json").read_bytes())["version"],
                "Production restoration requires the current committed firmware version")
        require(report["regions"]["bootloader"] in read_json((ROOT / "release/rollback-bootloaders.json").read_bytes())["sha256"],
                "Production restoration requires the transitioned bootloader")
    return dict(schema=1, snapshot=str(directory), snapshot_sha256=digest((directory / "snapshot.json").read_bytes()),
                mac=report["mac"], application=str(application.resolve()), application_sha256=digest(app),
                application_bytes=len(app), offset=report["selection"]["offset"],
                bootloader=str(bootloader.resolve()) if bootloader else None,
                bootloader_sha256=digest(boot) if bootloader else None)


def apply(device, plan, idf, expected_mac):
    directory = private_directory(Path(plan["snapshot"]))
    report_bytes = read_file(directory / "snapshot.json", 65536)
    require(plan["schema"] == 1 and plan["mac"] == expected_mac.lower() and
            digest(report_bytes) == plan["snapshot_sha256"], "Transition plan identity changed")
    rebuilt = make_plan(directory, Path(plan["application"]), Path(plan["bootloader"]) if plan["bootloader"] else None)
    require(rebuilt == plan, "Transition inputs changed; review a new plan")
    require(not (directory / "apply-started.json").exists(), "Previous write attempt exists; reconcile it before any retry")
    current = {name: device.read(address, size) for name, (address, size) in REGIONS.items()}
    report = analyze(current, idf)
    baseline = read_json(report_bytes)
    require(report["regions"] == baseline["regions"], "Device state changed since the reviewed snapshot")
    for name, (address, size) in REGIONS.items():
        require(device.read(address, size) == current[name], f"Fresh paired {name} reads differ")
    app = read_file(Path(plan["application"]), 1572864)
    boot = read_file(Path(plan["bootloader"]), 0x8000) if plan["bootloader"] else None
    require(digest(app) == plan["application_sha256"] and
            (boot is None or digest(boot) == plan["bootloader_sha256"]), "Transition input changed before writing")
    write_private(directory / "apply-started.json", json_bytes(plan))
    # Application first; retain ROM USB recovery throughout the bootloader write.
    device.write_verified(plan["offset"], app)
    if plan["bootloader"]:
        device.write_verified(0, boot)
    after = {name: device.read(address, size) for name, (address, size) in REGIONS.items()}
    protected = ("control", "audio", f"app{1-baseline['selection']['slot']}")
    require(all(after[name] == current[name] for name in protected), "Protected flash changed; keep the device stopped")
    if not plan["bootloader"]:
        require(after["bootloader"] == current["bootloader"], "Bootloader changed unexpectedly")
    require(after[f"app{baseline['selection']['slot']}"][:len(app)] == app, "Application readback mismatch")
    write_private(directory / "apply-verified.json", json_bytes(dict(plan=plan, utc=datetime.now(timezone.utc).isoformat())))


def preservation(baseline, current):
    require(baseline["records"] == current["records"], "Production record payloads changed")
    require(baseline["regions"]["audio"] == current["regions"]["audio"], "Shared recordings changed")


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    inspect = commands.add_parser("inspect")
    inspect.add_argument("--directory", type=Path, required=True)
    inspect.add_argument("--port", required=True)
    inspect.add_argument("--expect-mac", required=True)
    inspect.add_argument("--idf", type=Path, required=True)
    planning = commands.add_parser("plan")
    planning.add_argument("--snapshot", type=Path, required=True)
    planning.add_argument("--application", type=Path, required=True)
    planning.add_argument("--bootloader", type=Path)
    planning.add_argument("--output", type=Path, required=True)
    applying = commands.add_parser("apply")
    applying.add_argument("--plan", type=Path, required=True)
    applying.add_argument("--port", required=True)
    applying.add_argument("--expect-mac", required=True)
    applying.add_argument("--idf", type=Path, required=True)
    verifying = commands.add_parser("compare")
    verifying.add_argument("--baseline", type=Path, required=True)
    verifying.add_argument("--current", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "plan":
            require(not args.output.resolve().is_relative_to(ROOT.resolve()), "Keep plans outside Git")
            result = make_plan(args.snapshot, args.application, args.bootloader)
            write_private(args.output, json_bytes(result))
            print(json.dumps(result, indent=2))
        elif args.command == "compare":
            preservation(read_json(args.baseline.read_bytes()), read_json(args.current.read_bytes()))
            print("Production records and complete shared audio hashes match")
        else:
            require(re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", args.expect_mac), "Invalid expected MAC")
            device = Device(args.port, args.expect_mac)
            try:
                if args.command == "inspect":
                    result = capture(device, args.directory, args.idf, args.expect_mac)
                    print(json.dumps({key: result[key] for key in ("selection", "application_bytes", "application_sha256")}, indent=2))
                else:
                    apply(device, read_json(read_file(args.plan, 65536)), args.idf, args.expect_mac)
                    print("Scoped write independently verified; device remains stopped")
            finally:
                device.close()
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}. Keep the device stopped and reconcile before another write.\n")


if __name__ == "__main__":
    main()

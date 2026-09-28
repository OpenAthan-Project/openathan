#!/usr/bin/env python3
"""Build, package, and explicitly upload OpenAthan draft releases; never flash or publish."""
import argparse
from contextlib import contextmanager
from importlib.metadata import version
import io
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

import audio_image
import check_feasibility
from release_artifacts import (ASSETS, CONFIGURATION, LICENSE_PATH, MEDIA_REGISTRY, PINS_PATH,
                               REPOSITORY, SHA, TAG, approved_tracks, checksums, digest,
                               json_bytes, make_manifest, read_file, read_json, require,
                               validate_bundle)

ROOT = Path(__file__).resolve().parents[1]


def command(args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, **kwargs).stdout


def git(*args):
    return command(["git", "-C", ROOT, *args])


def source_file(commit, name):
    return git("show", f"{commit}:{name}")


def source_commit(commit):
    require(isinstance(commit, str) and SHA.fullmatch(commit), "--commit must be a full lowercase 40-character SHA")
    require(git("cat-file", "-t", commit).strip() == b"commit", "Source must identify a commit")
    return commit


@contextmanager
def fresh_output(destination):
    # Reject symlinks before resolving, including dangling links.
    require(not destination.is_symlink(), "Output must not be a symlink")
    destination = destination.resolve()
    require(not destination.is_relative_to(ROOT.resolve()), "Keep release output outside the repository")
    require(not destination.exists(), "Output already exists; choose a fresh directory")
    require(destination.parent.is_dir(), "Create the output parent directory first")
    with tempfile.TemporaryDirectory(prefix=f".{destination.name}-", dir=destination.parent) as temporary:
        stage = Path(temporary) / "result"
        stage.mkdir()
        yield stage
        # mkdir is an exclusive reservation: do not replace another process's output.
        destination.mkdir()
        try:
            stage.replace(destination)
        except BaseException:
            destination.rmdir()
            raise


def export_source(commit, destination):
    archive = git("archive", "--format=tar", commit)
    destination.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as handle:
        require(all(member.isfile() or member.isdir() for member in handle.getmembers()),
                "Release source must not contain symlinks or special files")
        handle.extractall(destination, filter="data")
    return archive


def build(commit, output):
    source_commit(commit)
    require(sys.version_info[:2] == (3, 13), "Use the pinned Python 3.13 environment")
    pins = read_json(source_file(commit, PINS_PATH))
    require(version("esphome") == pins["esphome"], "Install the source commit's pinned ESPHome requirements")
    with fresh_output(output) as stage:
        source = stage / "source"
        archive = export_source(commit, source)
        (stage / "source.tar").write_bytes(archive)
        environment = os.environ.copy()
        # ESPHome must not inherit an operator-selected build directory.
        environment["ESPHOME_BUILD_PATH"] = str(stage / "compiled")
        log = stage / "compile.log"
        print(f"Compiling {commit}; build output will be retained in {output}", flush=True)
        with log.open("wb") as handle:
            completed = subprocess.run([sys.executable, "-m", "esphome", "compile", CONFIGURATION],
                                       cwd=source, env=environment, stdout=handle, stderr=subprocess.STDOUT)
        if completed.returncode:
            # Include a bounded tail before temporary failed output is removed.
            raise ValueError("Firmware compilation failed:\n" + log.read_text(errors="replace")[-8000:])
        compiled = stage / "compiled/openathan"
        capacity = check_feasibility.inspect_firmware(compiled, log, source)
        for name in ("firmware.factory.bin", "firmware.ota.bin"):
            shutil.copyfile(compiled / "build" / name, stage / name)
        record = dict(schema=1, commit=commit, source_tree=git("rev-parse", f"{commit}^{{tree}}").decode().strip(),
                      configuration=CONFIGURATION, capacity=capacity,
                      toolchain=dict(python=".".join(map(str, sys.version_info[:3])), esphome=version("esphome"),
                                     esp_idf=pins["esp_idf"], compiler=pins["xtensa_esp_elf"], tzdata=version("tzdata")))
        validate_report(public_report(record), commit)
        record["evidence"] = {name: digest((stage / name).read_bytes()) for name in
                              ("source.tar", "compile.log", "firmware.factory.bin", "firmware.ota.bin")}
        (stage / "build-record.json").write_bytes(json_bytes(record))
    return record


def validate_build(directory):
    record = read_json(read_file(directory / "build-record.json", 65536))
    require(isinstance(record, dict), "Invalid build record")
    require(record.get("schema") == 1 and record.get("configuration") == CONFIGURATION, "Unsupported build record")
    commit = source_commit(record.get("commit"))
    require(record.get("source_tree") == git("rev-parse", f"{commit}^{{tree}}").decode().strip(), "Build source tree differs")
    evidence = record.get("evidence", {})
    require(isinstance(evidence, dict), "Invalid build evidence")
    require(set(evidence) == {"source.tar", "compile.log", "firmware.factory.bin", "firmware.ota.bin"},
            "Build evidence is incomplete")
    for name, sha in evidence.items():
        require(digest(read_file(directory / name, 64 * 1024 * 1024)) == sha, f"Build evidence changed: {name}")
    require((directory / "source.tar").read_bytes() == git("archive", "--format=tar", commit),
            "Archived source does not match the selected commit")
    # Check retained compilation output again against the original commit, not the
    # operator's checkout. Source metadata below is also read directly from Git.
    with tempfile.TemporaryDirectory(prefix="openathan-release-check-") as temporary:
        source = Path(temporary) / "source"
        export_source(commit, source)
        capacity = check_feasibility.inspect_firmware(directory / "compiled/openathan", directory / "compile.log", source)
    require(capacity == record.get("capacity"), "Build capacity evidence changed")
    for name in ("firmware.factory.bin", "firmware.ota.bin"):
        require((directory / name).read_bytes() == (directory / "compiled/openathan/build" / name).read_bytes(),
                "Retained and compiled firmware differ")
    validate_report(public_report(record), commit)
    return record


def public_report(record):
    return {key: record[key] for key in ("schema", "commit", "source_tree", "configuration", "toolchain", "capacity")}


def validate_report(report, commit):
    # Exact keys and source-reviewed values prevent raw logs/paths from entering uploads.
    require(set(report) == {"schema", "commit", "source_tree", "configuration", "toolchain", "capacity"},
            "Unexpected public build-report fields")
    pins = read_json(source_file(commit, PINS_PATH))
    toolchain = report.get("toolchain", {})
    require(isinstance(toolchain, dict), "Invalid toolchain report")
    require(set(toolchain) == {"python", "esphome", "esp_idf", "compiler", "tzdata"}, "Invalid toolchain report")
    require(isinstance(toolchain["python"], str) and re.fullmatch(r"3\.13\.\d+", toolchain["python"]), "Invalid Python version")
    require(toolchain["esphome"] == pins["esphome"] and toolchain["esp_idf"] == pins["esp_idf"] and
            toolchain["compiler"] == pins["xtensa_esp_elf"] and toolchain["tzdata"] == "2026.4", "Toolchain report differs from pins")
    require(report["source_tree"] == git("rev-parse", f"{commit}^{{tree}}").decode().strip(), "Report source tree differs")
    capacity = report.get("capacity", {})
    keys = {"application_bytes", "linked_image_bytes", "static_ram_bytes", "application_slot_bytes", "slot_free_bytes",
            "growth_target_passed", "application_sha256", "application_budget_remaining_bytes", "managed_components"}
    require(isinstance(capacity, dict) and capacity.get("growth_target_passed") is True, "Invalid capacity report")
    require(set(capacity) == keys, "Unexpected capacity report fields")
    for key in keys - {"growth_target_passed", "application_sha256", "managed_components"}:
        require(type(capacity[key]) is int and capacity[key] >= 0, "Invalid capacity measurement")
    expected = {**pins["managed_components"], **pins["optional_managed_components"]}
    require(capacity["managed_components"] == expected, "Reference build dependencies differ")


def validate_audio_cpp(commit, path):
    # Always compile the production format validator from the bundle's source
    # revision, rather than executing a pre-existing binary supplied in a bundle.
    with tempfile.TemporaryDirectory(prefix="openathan-audio-validator-") as temporary:
        source = Path(temporary) / "source"
        export_source(commit, source)
        build_dir = Path(temporary) / "build"
        command(["cmake", "-S", source, "-B", build_dir])
        command(["cmake", "--build", build_dir, "--target", "audio_validator", "--parallel", "2"])
        command([build_dir / "audio_validator", path.resolve()])


def package(build_dir, tag, normal, fajr, output):
    require(TAG.fullmatch(tag), "Version must have the form vX.Y.Z")
    record = validate_build(build_dir)
    commit = record["commit"]
    payloads = (audio_image.read_recording(normal), audio_image.read_recording(fajr))
    approved_tracks(read_json(source_file(commit, MEDIA_REGISTRY)), source_file(commit, LICENSE_PATH), payloads)
    media, _ = audio_image.build_image(*payloads)
    factory = (build_dir / ASSETS[1]).read_bytes()
    with fresh_output(output) as stage:
        files = {ASSETS[1]: factory, ASSETS[2]: media,
                 "manifest.json": json_bytes(make_manifest(commit, tag, factory, media)),
                 "build-report.json": json_bytes(public_report(record))}
        files["SHA256SUMS"] = checksums(files)
        for name, data in files.items():
            (stage / name).write_bytes(data)
        validate_bundle(stage)
        validate_audio_cpp(commit, stage / ASSETS[2])
    return dict(bundle=str(output), manifest_sha256=digest(files["manifest.json"]))


class GitHub:
    def call(self, *args):
        environment = os.environ.copy()
        environment["GH_HOST"] = "github.com"
        return command(["gh", *args], env=environment)

    def api(self, path, paginate=False):
        args = ["api", f"repos/{REPOSITORY}/{path}"]
        if paginate:
            args += ["--paginate", "--slurp"]
        result = read_json(self.call(*args))
        return [entry for page in result for entry in page] if paginate else result

    def tag_commit(self, tag):
        obj = self.api(f"git/ref/tags/{tag}")["object"]
        for _ in range(8):
            if obj["type"] == "commit":
                return obj["sha"]
            require(obj["type"] == "tag" and SHA.fullmatch(obj["sha"]), "Unsupported tag target")
            obj = self.api(f"git/tags/{obj['sha']}")["object"]
        raise ValueError("Tag indirection is too deep")

    def asset_bytes(self, asset):
        return self.call("api", f"repos/{REPOSITORY}/releases/assets/{asset['id']}",
                         "-H", "Accept: application/octet-stream")


def release_gate(github, commit, tag):
    require(github.tag_commit(tag) == commit, "Remote version tag must exist and match the bundle commit")
    comparison = github.api(f"compare/{commit}...main")
    require(comparison["merge_base_commit"]["sha"] == commit, "Release source is not on main")
    runs = github.api(f"actions/workflows/firmware.yml/runs?head_sha={commit}&event=push&branch=main&per_page=100")
    matches = [run for run in runs["workflow_runs"] if run.get("head_sha") == commit and
               run.get("head_branch") == "main" and run.get("event") == "push" and
               run.get("head_repository", {}).get("full_name") == REPOSITORY]
    require(matches, "No main-branch Firmware CI run exists for this commit")
    latest = max(matches, key=lambda run: run["id"])
    require(latest.get("status") == "completed" and latest.get("conclusion") == "success",
            "The latest Firmware CI run for this commit has not passed")


def find_release(github, tag):
    found = [item for item in github.api("releases?per_page=100", paginate=True) if item["tag_name"] == tag]
    require(len(found) <= 1, "Multiple releases use this version tag")
    if found:
        require(found[0]["draft"] is True and found[0].get("published_at") is None and
                found[0].get("prerelease") is False, "Existing release is published or is not a stable-version draft")
    return found[0] if found else None


def verify_assets(github, release, files):
    found = {}
    for asset in github.api(f"releases/{release['id']}/assets?per_page=100", paginate=True):
        name = asset["name"]
        require(name in files and name not in found, "Draft contains unexpected or duplicate assets")
        require(asset.get("state") == "uploaded" and asset["size"] == len(files[name]), f"Conflicting draft asset: {name}")
        require(github.asset_bytes(asset) == files[name], f"Conflicting draft asset: {name}")
        found[name] = asset
    return found


def upload_draft(bundle, github=None):
    # Snapshot locally first: later edits cannot change the bytes being uploaded.
    manifest, report, files, payloads = validate_bundle(bundle)
    commit, tag = manifest["commit"], manifest["tag"]
    source_commit(commit)
    validate_report(report, commit)
    approved_tracks(read_json(source_file(commit, MEDIA_REGISTRY)), source_file(commit, LICENSE_PATH), payloads)
    github = github or GitHub()
    with tempfile.TemporaryDirectory(prefix="openathan-upload-") as temporary:
        snapshot = Path(temporary)
        for name, data in files.items():
            (snapshot / name).write_bytes(data)
        validate_audio_cpp(commit, snapshot / ASSETS[2])
        github.call("auth", "status", "--hostname", "github.com")
        release_gate(github, commit, tag)
        release = find_release(github, tag)
        if release is None:
            notes = snapshot / "notes.md"
            notes.write_text(f"Reference firmware candidate from `{commit}`.\n\n"
                             "Recording redistribution metadata was checked against the committed registry. "
                             "Physical qualification and public publication remain pending. "
                             "The website installer requires a separate reviewed release selection.\n\n"
                             f"Manifest SHA-256: `{digest(files['manifest.json'])}`\n")
            github.call("release", "create", tag, "--repo", REPOSITORY, "--draft", "--verify-tag", "--latest=false",
                        "--title", f"OpenAthan {tag}", "--notes-file", notes)
            release = find_release(github, tag)
            require(release is not None, "Draft creation outcome is uncertain; rerun to reconcile")
        found = verify_assets(github, release, files)
        for name in ASSETS:
            if name not in found:
                # Recheck state/tag before each mutation; never clobber assets.
                require(find_release(github, tag)["id"] == release["id"], "Draft identity changed")
                require(github.tag_commit(tag) == commit, "Remote tag changed during upload")
                github.call("release", "upload", tag, snapshot / name, "--repo", REPOSITORY)
        require(find_release(github, tag)["id"] == release["id"], "Draft identity changed")
        require(github.tag_commit(tag) == commit, "Remote tag changed during upload")
        require(set(verify_assets(github, release, files)) == set(ASSETS), "Upload is incomplete; rerun to reconcile")
    return dict(url=release["html_url"], manifest_sha256=digest(files["manifest.json"]), status="draft",
                remaining="Hardware qualification, explicit publication, and reviewed website release selection")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    build_parser = commands.add_parser("build", help="Compile an exact source commit without recordings")
    build_parser.add_argument("--commit", required=True)
    build_parser.add_argument("--output-dir", required=True, type=Path)
    package_parser = commands.add_parser("package", help="Package firmware with approved recordings")
    package_parser.add_argument("--build-dir", required=True, type=Path)
    package_parser.add_argument("--tag", required=True)
    package_parser.add_argument("--normal", required=True, type=Path)
    package_parser.add_argument("--fajr", required=True, type=Path)
    package_parser.add_argument("--output-dir", required=True, type=Path)
    upload_parser = commands.add_parser("upload-draft", help="Upload approved assets to a GitHub draft; never publish")
    upload_parser.add_argument("--bundle", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.action == "build":
            result = build(args.commit, args.output_dir)
        elif args.action == "package":
            result = package(args.build_dir.resolve(), args.tag, args.normal, args.fajr, args.output_dir)
        else:
            result = upload_draft(args.bundle)
        print(json.dumps(result, indent=2))
    except subprocess.CalledProcessError as error:
        parser.exit(1, f"Command failed ({Path(str(error.cmd[0])).name}). "
                       "For uploads, the outcome may be partial; rerun to reconcile without replacing assets.\n" +
                       (error.stderr or b"").decode(errors="replace")[-4000:] + "\n")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Release preparation stopped: {error}\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Omit firmware compilation only for verified documentation-only PR merges."""
import json
import os
from pathlib import Path, PurePosixPath
import subprocess


# Explicit prose-only paths outside docs/. New paths default to full builds.
# AUDIO-LICENSES.md is a release input; third-party metadata is also excluded.
DOCUMENTATION = frozenset({
    ".github/pull_request_template.md", ".github/workflows/README.md",
    "AGENTS.md", "CODE_OF_CONDUCT.md", "CONTRIBUTING.md", "PRODUCT.md",
    "README.md", "SECURITY.md",
    "firmware/esphome/README.md",
    "firmware/esphome/components/openathan/README.md",
    "firmware/esphome/components/voice_pyramid/README.md",
    "firmware/esphome/feasibility/README.md",
    "firmware/esphome/provisioning/HARDWARE_TEST.md",
    "firmware/esphome/provisioning/README.md",
    "firmware/esphome/scheduler/README.md",
    "firmware/esphome/scheduler/SETTINGS.md",
    "firmware/esphome/scheduler/VALIDATION.md",
    "firmware/esphome/upgrades/VALIDATION.md",
    "hardware/LICENSE.md", "hardware/expansions/README.md",
    "hardware/reference-builds/voice-pyramid/assembly.md",
    "hardware/reference-builds/voice-pyramid/bom.md",
    "lib/openathan-core/README.md", "web/device-ui/README.md",
})


def is_documentation(path):
    parsed = PurePosixPath(path)
    if str(parsed) != path or ".." in parsed.parts:
        return False
    return path in DOCUMENTATION or (path.startswith("docs/") and parsed.suffix == ".md")


def git(root, *arguments):
    return subprocess.run(
        ["git", *arguments], cwd=root, check=True, capture_output=True,
        timeout=30,
    ).stdout.decode("utf-8")


def classify(root, environment):
    """Return (build_required, reason), defaulting to compilation on uncertainty."""
    if environment.get("GITHUB_EVENT_NAME") != "pull_request":
        return True, "Full firmware checks are required for non-PR events."
    try:
        event = json.loads(Path(environment["GITHUB_EVENT_PATH"]).read_text())
        request = event["pull_request"]
        parents = git(root, "show", "-s", "--format=%P", "HEAD").strip().split()
        if (git(root, "rev-parse", "HEAD").strip() != environment["GITHUB_SHA"]
                or parents != [request["base"]["sha"], request["head"]["sha"]]):
            return True, "Unverified PR merge identity; running full firmware checks."
        changed = git(root, "diff", "--no-ext-diff", "--no-renames", "--name-only",
                      "-z", "HEAD^1", "HEAD", "--")
        if not changed.endswith("\0"):
            return True, "Empty or unexpected diff; running full firmware checks."
        paths = changed[:-1].split("\0")
        if not all(is_documentation(path) for path in paths):
            return True, "Changes include firmware inputs or unrecognized paths."
        return False, "Documentation-only PR: firmware compilation intentionally omitted."
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError):
        return True, "Change detection was unavailable; running full firmware checks."


def main():
    required, reason = classify(Path.cwd(), os.environ)
    with Path(os.environ["GITHUB_OUTPUT"]).open("a") as output:
        output.write(f"build_required={str(required).lower()}\n")
    print(reason)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a") as summary:
            summary.write(f"{reason}\n")


if __name__ == "__main__":
    main()

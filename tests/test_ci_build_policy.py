"""Exercise documentation-only build decisions against real PR merge trees."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import ci_build_policy as policy


class BuildPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "repo"
        self.root.mkdir()
        self.git("init", "-b", "main")
        self.git("config", "user.name", "CI Test")
        self.git("config", "user.email", "ci@example.invalid")
        self.write("README.md", "initial documentation\n")
        self.write("firmware/example.cpp", "initial code\n")
        self.commit("initial")
        self.git("checkout", "-b", "feature")
        self.event_path = Path(self.temporary.name) / "event.json"

    def git(self, *args):
        return subprocess.run(["git", *args], cwd=self.root, check=True,
                              capture_output=True, text=True).stdout.strip()

    def write(self, path, text="new content\n"):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-m", message)

    def merge(self):
        self.commit("PR changes")
        head = self.git("rev-parse", "HEAD")
        self.git("checkout", "main")
        self.write("base-only", "unrelated base change\n")
        self.commit("base advance")
        base = self.git("rev-parse", "HEAD")
        self.git("merge", "--no-ff", "feature", "-m", "PR merge")
        self.event_path.write_text(json.dumps({"pull_request": {
            "base": {"sha": base}, "head": {"sha": head},
        }}))
        return {"GITHUB_EVENT_NAME": "pull_request",
                "GITHUB_EVENT_PATH": str(self.event_path),
                "GITHUB_SHA": self.git("rev-parse", "HEAD")}

    def required(self, environment):
        return policy.classify(self.root, environment)[0]

    def test_documentation_edits_additions_deletions_and_renames(self):
        self.write("docs/new guide.md")
        self.write("README.md", "edited documentation\n")
        self.git("mv", "README.md", "docs/renamed.md")
        self.write("firmware/esphome/README.md")
        self.assertFalse(self.required(self.merge()))

    def test_deletion_only(self):
        (self.root / "README.md").unlink()
        self.assertFalse(self.required(self.merge()))

    def test_large_diff_and_unusual_filenames(self):
        for number in range(350):
            self.write(f"docs/guide {number}.md")
        self.write("docs/line\nbreak.md")
        self.assertFalse(self.required(self.merge()))

    def test_mixed_changes(self):
        self.write("README.md", "edited\n")
        self.write("firmware/example.cpp", "changed code\n")
        self.assertTrue(self.required(self.merge()))

    def test_rename_from_code_to_documentation(self):
        (self.root / "docs").mkdir()
        self.git("mv", "firmware/example.cpp", "docs/example.md")
        self.assertTrue(self.required(self.merge()))

    def test_rename_from_documentation_to_unknown_path(self):
        self.git("mv", "README.md", "new-guide.md")
        self.assertTrue(self.required(self.merge()))

    def test_unknown_and_release_paths(self):
        for path in ("AUDIO-LICENSES.md", "third_party/adhan-cpp/UPSTREAM.md",
                     "unknown.md", "docs/input.json", "tools/check.py",
                     ".github/workflows/firmware.yml"):
            self.assertFalse(policy.is_documentation(path), path)
        self.write("AUDIO-LICENSES.md")
        self.assertTrue(self.required(self.merge()))

    def test_push_always_builds(self):
        self.write("README.md", "edited\n")
        environment = self.merge()
        environment["GITHUB_EVENT_NAME"] = "push"
        self.assertTrue(self.required(environment))

    def test_mismatched_identity_and_malformed_event(self):
        self.write("README.md", "edited\n")
        environment = self.merge()
        self.assertTrue(self.required({**environment, "GITHUB_SHA": "0" * 40}))
        event = json.loads(self.event_path.read_text())
        for parent in ("base", "head"):
            changed = json.loads(json.dumps(event))
            changed["pull_request"][parent]["sha"] = "0" * 40
            self.event_path.write_text(json.dumps(changed))
            self.assertTrue(self.required(environment))
        for invalid in ("not json", "{}", "[]"):
            self.event_path.write_text(invalid)
            self.assertTrue(self.required(environment))
        self.event_path.unlink()
        self.assertTrue(self.required(environment))

    def test_empty_merge_diff(self):
        self.git("commit", "--allow-empty", "-m", "empty PR")
        head = self.git("rev-parse", "HEAD")
        self.git("checkout", "main")
        base = self.git("rev-parse", "HEAD")
        self.git("merge", "--no-ff", "feature", "-m", "empty merge")
        self.event_path.write_text(json.dumps({"pull_request": {
            "base": {"sha": base}, "head": {"sha": head}}}))
        self.assertTrue(self.required({"GITHUB_EVENT_NAME": "pull_request",
                                      "GITHUB_EVENT_PATH": str(self.event_path),
                                      "GITHUB_SHA": self.git("rev-parse", "HEAD")}))

    def test_shallow_checkout_and_missing_history(self):
        self.write("README.md", "edited\n")
        environment = self.merge()
        for depth, required in ((2, False), (1, True)):
            clone = Path(self.temporary.name) / f"depth-{depth}"
            subprocess.run(["git", "clone", "--depth", str(depth),
                            self.root.as_uri(), str(clone)], check=True,
                           capture_output=True)
            self.assertEqual(policy.classify(clone, environment)[0], required)

    def test_cli_outputs_and_summary(self):
        self.write("README.md", "edited\n")
        environment = self.merge()
        output = Path(self.temporary.name) / "output"
        summary = Path(self.temporary.name) / "summary"
        subprocess.run([sys.executable, str(ROOT / "tools/ci_build_policy.py")],
                       cwd=self.root, check=True, capture_output=True,
                       env={**os.environ, **environment, "GITHUB_OUTPUT": str(output),
                            "GITHUB_STEP_SUMMARY": str(summary)})
        self.assertEqual(output.read_text(), "build_required=false\n")
        self.assertIn("intentionally omitted", summary.read_text())

    def test_git_timeout_falls_back_to_build(self):
        self.write("README.md", "edited\n")
        environment = self.merge()
        with patch.object(policy, "git", side_effect=subprocess.TimeoutExpired("git", 30)):
            self.assertTrue(self.required(environment))


class WorkflowPolicyTests(unittest.TestCase):
    def test_all_expensive_steps_require_successful_docs_only_detection_to_skip(self):
        import yaml
        workflow = yaml.safe_load((ROOT / ".github/workflows/firmware.yml").read_text())
        build = workflow["jobs"]["firmware-build"]
        self.assertNotIn("if", build)
        self.assertEqual(build["strategy"]["matrix"]["configuration"], [
            "device", "validation", "reference", "provisioning-validation",
            "upgrade-qualification", "upgrade-startup-failure", "waveshare-production", "waveshare-audio", "waveshare-display", "waveshare-diagnostics"])
        steps = build["steps"]
        self.assertEqual(steps[0]["with"]["fetch-depth"], 2)
        detector = next(step for step in steps if step.get("id") == "build_policy")
        self.assertTrue(detector["continue-on-error"])
        expensive = [step for step in steps if step.get("name") in (
            "Install build dependencies", "Compile and inspect firmware",
            "Test settings JSON with the resolved firmware library",
            "Test update request formatting with the resolved SDK")]
        self.assertEqual(len(expensive), 4)
        for step in expensive:
            self.assertIn("steps.build_policy.outcome != 'success'", step["if"])
            self.assertIn("steps.build_policy.outputs.build_required != 'false'", step["if"])
        for job in ("host-tests", "device-ui"):
            self.assertNotIn("if", workflow["jobs"][job])
            self.assertTrue(all("build_policy" not in step.get("if", "")
                                for step in workflow["jobs"][job]["steps"]))

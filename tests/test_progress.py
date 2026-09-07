import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("progress", ROOT / "tools/progress.py")
progress = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(progress)


class ProgressTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.readme = Path(self.directory.name) / "README.md"
        self.expected = progress.readme_progress_block(13237)
        self.stale = self.expected.replace(f"{progress.NATIVE_SITES:,}", f"{progress.NATIVE_SITES - 1:,}", 1)

    def tearDown(self):
        self.directory.cleanup()

    def invoke(self, *args, denominator=13237):
        with mock.patch.object(progress, "README", self.readme), mock.patch.object(
            progress, "discovered_instruction_variants", return_value=(denominator, "test")
        ), mock.patch.object(sys, "argv", ["progress.py", *args]):
            return progress.main()

    def test_check_detects_stale_without_mutating(self):
        self.readme.write_text(self.stale)
        before = self.readme.read_text()
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(self.invoke("--check-readme"), 1)
        self.assertIn("stale", output.getvalue())
        self.assertEqual(self.readme.read_text(), before)

    def test_update_is_idempotent_and_check_passes(self):
        self.readme.write_text("before\n" + self.stale + "\nafter\n")
        self.assertEqual(self.invoke("--update-readme"), 0)
        updated = self.readme.read_text()
        self.assertIn(self.expected, updated)
        self.assertEqual(self.invoke("--update-readme"), 0)
        self.assertEqual(self.readme.read_text(), updated)
        self.assertEqual(self.invoke("--check-readme"), 0)

    def test_update_does_not_overwrite_missing_block(self):
        self.readme.write_text("no tracker here\n")
        before = self.readme.read_text()
        self.assertEqual(self.invoke("--update-readme"), 1)
        self.assertEqual(self.readme.read_text(), before)

    def test_default_and_json_outputs_remain_valid(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(self.invoke(), 0)
        self.assertIn("Verified native reconstruction:", output.getvalue())
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(self.invoke("--json"), 0)
        result = json.loads(output.getvalue())
        self.assertEqual(result["headline"]["completed"], progress.NATIVE_SITES)
        self.assertEqual(result["headline"]["total"], 13237)


if __name__ == "__main__":
    unittest.main()

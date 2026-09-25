#!/usr/bin/env python3
"""Run the complete Python suite, treating skipped tests as a failure."""
from pathlib import Path
import sys
import unittest

root = Path(__file__).resolve().parents[1]
suite = unittest.defaultTestLoader.discover(str(root / "tests"))
result = unittest.TextTestRunner(verbosity=2).run(suite)
if result.skipped:
    print("Skipped tests are not a validation pass; use the pinned ESPHome environment.", file=sys.stderr)
sys.exit(0 if result.wasSuccessful() and not result.skipped else 1)

#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path
import subprocess
import sys

# Get the absolute path of `chromium/src/`, deduced from the absolute
# path of this file.
CHROMIUM_SRC = Path(__file__).resolve().parents[4]
assert CHROMIUM_SRC.name == "src"

TEST_TOOL = CHROMIUM_SRC / "tools/clang/scripts/test_tool.py"
assert TEST_TOOL.is_file()

SPANIFY_PATH = CHROMIUM_SRC / "third_party/llvm-build/Release+Asserts/bin/spanify"


def RunRewritingTests():
    return subprocess.run(
        [TEST_TOOL, "--extract-edits-path", "..", "--apply-edits",
         "spanify"]).returncode


def main():
    if not SPANIFY_PATH.is_file():
        print("Please build spanify first", file=sys.stderr)
        return 1

    return RunRewritingTests()


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import subprocess
import sys


def RunRewritingTests():
    return subprocess.run([
        "tools/clang/scripts/test_tool.py", "--extract-edits-path", "..",
        "--apply-edits", "spanify"
    ]).returncode


def main():
    if not os.path.exists("ATL_OWNERS"):
        sys.stderr.write(
            "Please run run_all_tests.py from the root dir of Chromium")
        return -1

    if not os.path.exists("third_party/llvm-build/Release+Asserts/bin/"
                          "spanify"):
        sys.stderr.write("Please build spanify first")
        return -1

    return RunRewritingTests()


if __name__ == "__main__":
    sys.exit(main())

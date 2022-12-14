#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os.path
import shutil
import subprocess
import sys


def RunRewritingTests():
  subprocess.run([
      "tools/clang/scripts/test_tool.py", "--apply-edits",
      "rewrite_raw_ptr_fields"
  ])


def RunGeneratingTest(test_path):
  tmp_test_path = test_path.replace("-test.cc", "-original.cc")
  test_filter = os.path.basename(test_path).replace("-test.cc", "")

  shutil.copyfile(test_path, tmp_test_path)
  try:
    subprocess.run([
        "tools/clang/scripts/test_tool.py",
        "--test-filter=%s" % test_filter, "rewrite_raw_ptr_fields"
    ])
  finally:
    os.remove(tmp_test_path)


def RunGeneratingTests():
  tests = glob.glob("tools/clang/rewrite_raw_ptr_fields/tests/gen-*-test.cc")
  for test_path in tests:
    RunGeneratingTest(test_path)


def main():
  if not os.path.exists("ATL_OWNERS"):
    sys.stderr.write(
        "Please run run_all_tests.py from the root dir of Chromium")
    return -1

  if not os.path.exists(
      "third_party/llvm-build/Release+Asserts/bin/rewrite_raw_ptr_fields"):
    sys.stderr.write("Please build rewrite_raw_ptr_fields first")
    return -1

  RunRewritingTests()
  RunGeneratingTests()


if __name__ == "__main__":
  main()

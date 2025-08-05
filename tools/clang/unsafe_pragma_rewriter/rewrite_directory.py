#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to use clang complier errors to enclose UNSAFE_TODO() regions.

import argparse
import os
import re
import subprocess
import sys
import tempfile


def main():
  parser = argparse.ArgumentParser(
      description="Rewrite UNSAFE_TODO regions using clang compiler errors.",
      formatter_class=argparse.RawDescriptionHelpFormatter)

  parser.add_argument("-C",
                      dest="build_dir",
                      default="out/Debug",
                      help="Specify the build directory, defaults to out/Debug")
  parser.add_argument("-f",
                      dest="force",
                      action="store_true",
                      help="skip conditional compilation checks.")
  parser.add_argument("-v",
                      dest="verbose",
                      action="store_true",
                      help="Enable verbose logging")
  parser.add_argument("directory",
                      help="Directory under src/ to checkout and modify.")

  args = parser.parse_args()
  build_dir = args.build_dir
  directory = args.directory
  force = args.force
  verbose = args.verbose

  print("Checking GN build arg configuration ...")
  try:
    dcheck_cmd = [
        "gn", "args", "-C", build_dir, "--short", "--list=dcheck_always_on"
    ]
    dcheck = subprocess.check_output(dcheck_cmd, text=True)
    if "true" not in dcheck:
      print("Set GN arg dcheck_always_on = true", file=sys.stderr)
      sys.exit(1)

    diag_cmd = [
        "gn", "args", "-C", build_dir, "--short",
        "--list=diagnostics_print_source_range_info"
    ]
    diag = subprocess.check_output(diag_cmd, text=True)
    if "true" not in diag:
      print("Set GN arg diagnostics_print_source_range_info = true",
            file=sys.stderr)
      sys.exit(1)

    warn_cmd = [
        "gn", "args", "-C", build_dir, "--short",
        "--list=treat_warnings_as_errors"
    ]
    warn = subprocess.check_output(warn_cmd, text=True)
    if "false" not in warn:
      print("Set GN arg treat_warnings_as_errors = false", file=sys.stderr)
      sys.exit(1)

  except subprocess.CalledProcessError as e:
    print(f"Error checking GN args: {e.stderr}", file=sys.stderr)
    sys.exit(1)

  tmpdir = tempfile.mkdtemp(None, "unsafe_pragma_rewriter.")
  print(f"Temporary files will be written to {tmpdir}\n")

  try:
    grep_cmd = ["git", "grep", "-l", "^#pragma allow_unsafe_", directory]
    grep = subprocess.check_output(grep_cmd, text=True).strip()
  except Exception as e:
    print("No candidates found")
    sys.exit(1)

  grep_lines = grep.splitlines() if grep else []
  source_files = [x for x in grep_lines if re.match(r".*\.cc$", x)]
  if not source_files:
    print("No files met criteria")
    sys.exit(1)

  if verbose:
    print("Files containing unsafe pragmas:")
    print("\n".join(source_files), "\n")

  if not force:
    iffy_cmd = ["grep", "-Pc", "^#if(?! DCHECK_IS_ON\\(\\))"] + source_files
    iffy = subprocess.check_output(iffy_cmd, text=True).strip()
    iffy_lines = iffy.splitlines() if iffy else []
    iffy_files = [x.split(":")[0] for x in iffy_lines if x.split(":")[1] != "1"]
    if iffy_files:
      if verbose:
        print("Skipping conditionally-compiled files:")
        print("\n".join(iffy_files), "\n")
      source_files = [x for x in source_files if not x in set(iffy_files)]

    if not source_files:
      print("No remaining files")
      sys.exit(1)

    if verbose:
      print("Remaining files after excluding #ifdefs:")
      print("\n".join(source_files), "\n")

  # Starting with all files in the directory, find the ones that are
  # able to be compiled on this platform/configurarion by asking ninja
  # to build them all, and then removing the ones that aren't known.
  obj_targets = ["../../" + x + "^" for x in source_files]
  ninja_command = ["autoninja", "-C", build_dir] + obj_targets
  ninja = subprocess.run(ninja_command, text=True, capture_output=True)
  if ninja.stderr:
    source_files = [
        x for x in source_files
        if not 'unknown target "../../' + x in ninja.stderr
    ]

  if verbose:
    print("Remaining files after excluding unbuildable:")
    print("\n".join(source_files), "\n")

  subprocess.run(
      ["tools/clang/unsafe_pragma_rewriter/remove_unsafe_pragma.py"] +
      source_files,
      check=True)

  print("Compile to find unsafe errors ...")
  targets = ["../../" + x + "^" for x in source_files]
  buildlog0 = os.path.join(tmpdir, "buildlog0")
  with open(buildlog0, "w") as f_log:
    subprocess.run(["autoninja", "-k", "1000", "-C", build_dir, "-v"] + targets,
                   stdout=f_log,
                   stderr=subprocess.STDOUT)

  with open(buildlog0) as f_in:
    compiled = subprocess.check_output(
        ["tools/clang/unsafe_pragma_rewriter/extract_sources.py"],
        stdin=f_in,
        text=True).strip()
    compiled_files = compiled.splitlines() if compiled else []

  if not compiled_files:
    print("No modified files were compiled")
    sys.exit(1)

  if verbose:
    print("Set of files compiled")
    print("\n".join(compiled_files), "\n")

  source_files = [x for x in source_files if x in set(compiled_files)]
  if verbose:
    print("Set of modified files compiled")
    print("\n".join(compiled_files), "\n")

  with open(buildlog0) as f_in:
    fail = subprocess.check_output(
        ["tools/clang/unsafe_pragma_rewriter/extract_failures.py"],
        stdin=f_in,
        text=True).strip()
    fail_files = fail.splitlines() if fail else []

  if verbose and fail_files:
    print("Set of files with detected warnings")
    print("\n".join(fail_files), "\n")

  print("Resetting to clean state ...")
  subprocess.run(["git", "checkout", "--", directory], check=True)
  subprocess.run(
      ["tools/clang/unsafe_pragma_rewriter/remove_unsafe_pragma.py"] +
      source_files,
      check=True)

  print("Adding UNSAFE_TODO() ...")
  with open(buildlog0) as f_in:
    subprocess.run(["tools/clang/unsafe_pragma_rewriter/fix_unsafe.py"],
                   stdin=f_in,
                   check=True)

  if source_files:
    try:
      needs_header_cmd = ["git", "grep", "-l", "UNSAFE_TODO"] + source_files
      needs_header = subprocess.check_output(needs_header_cmd,
                                             text=True).strip()
      needs_header_files = needs_header.splitlines() if needs_header else []
    except Exception as e:
      needs_header_files = []

    if needs_header_files:
      subprocess.run(
          ["tools/add_header.py", "--header", '"base/compiler_specific.h"'] +
          needs_header_files,
          check=True)

  for i in range(1, 5):
    print(f"Compile to find bad rewrites (Pass {i}) ...")
    buildlog_i = os.path.join(tmpdir, f"buildlog{i}")
    with open(buildlog_i, "w") as f_log:
      subprocess.run(["autoninja", "-k", "1000", "-C", build_dir] + targets,
                     stdout=f_log,
                     stderr=subprocess.STDOUT)

    with open(buildlog_i) as f_in:
      fail = subprocess.check_output(
          ["tools/clang/unsafe_pragma_rewriter/extract_failures.py"],
          stdin=f_in,
          text=True).strip()
      failures = fail.splitlines() if fail else []

    if not failures:
      break

    if verbose:
      print("Failed to compile, reverting:")
      print("\n".join(failures), "\n")

    for failure in failures:
      subprocess.run(["git", "checkout", "--", failure], check=True)

  print("Formatting changes")
  subprocess.run(["git", "cl", "format"], check=True)

  print("Finished.")


if __name__ == "__main__":
  main()

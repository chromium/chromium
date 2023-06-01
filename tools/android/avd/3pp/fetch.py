#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Print a version and prepare the artifacts for "avd" CIPD package.

This script has the following two functions:
  * latest: Prints a version for 3pp framework to decide if creating a new CIPD
    package or not. The version value is calculated by computing the sha256
    hash of all the dependent artifacts. In this way, it is guaranteed that a
    new version will be generated if any of the dependencies get changed.
  * checkout: Prepares the dependent artifacts for 3pp framework to package and
    upload to CIPD by copying all the dependencies to a temporary checkout path
    created by 3pp framework.

This script is normally called by 3pp framework from chromium_3pp recipe module.

"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir,
                 os.path.pardir, os.path.pardir))
# The src-relative files and dirs we would like to include in the CIPD.
_BASE_DEPS = [
    # vpython, binaries and avd configs used by //tools/android/avd/avd.py
    '.vpython3',
    'third_party/android_sdk/public/cmdline-tools/',
    'third_party/android_sdk/public/platform-tools/',
    'tools/android/avd/proto/',

    # Should be the same as python_library("devil_chromium_py") in
    # //build/android/BUILD.gn
    'build/android/devil_chromium.json',
    'third_party/catapult/devil/devil/devil_dependencies.json',
    'third_party/catapult/third_party/gsutil/',

    # Read by gn_helpers.BuildWithChromium()
    # Needed to recognize the adb binary in third_party/android_sdk
    'build/config/gclient_args.gni',
]


def _file_hash(sha, rel_path, base_path):
  path = os.path.join(base_path, rel_path)
  with open(path, 'rb') as f:
    sha.update(str(len(rel_path)).encode('utf-8'))
    sha.update(rel_path.encode('utf-8'))
    while True:
      f_stream = f.read(4096)
      if not f_stream:
        break
      sha.update(str(len(f_stream)).encode('utf-8'))
      sha.update(f_stream)


# This function computes sha256 hash of provided directories and/or files that
# are relative to the given base_path.
# Copied from the compute_hash function in https://chromium.googlesource.com/
# infra/luci/recipes-py/+/HEAD/recipe_modules/file/resources/fileutil.py
def _compute_hash_paths(base_path, *rel_paths):
  sha = hashlib.sha256()
  for rel_path in rel_paths:
    path = os.path.join(base_path, rel_path)
    if os.path.isfile(path):
      _file_hash(sha, rel_path, base_path)
    elif os.path.isdir(path):
      for root, dirs, files in os.walk(path, topdown=True):
        dirs.sort()  # ensure we walk dirs in sorted order
        files.sort()
        for f_name in files:
          f_path = os.path.join(root, f_name)
          # Check if it's a file to prevent following symlinks.
          if os.path.isfile(f_path):
            rel_file_path = os.path.relpath(f_path, base_path)
            _file_hash(sha, rel_file_path, base_path)

  return sha.hexdigest()


# Return a list of src-relative paths for the dependent files and dirs for avd
def _get_deps(chromium_src_path):
  deps_cmds = [
      os.path.join(chromium_src_path, 'build', 'print_python_deps.py'),
      '--root',
      chromium_src_path,
      os.path.join(chromium_src_path, 'tools', 'android', 'avd', 'avd.py'),
  ]
  deps_output = subprocess.check_output(deps_cmds, universal_newlines=True)
  # Filter out comments in deps_output
  deps_lines = deps_output.strip().split('\n')
  deps_entries = [dep for dep in deps_lines if not dep.startswith('#')]
  return sorted(_BASE_DEPS + deps_entries)


def do_latest():
  deps = _get_deps(_SRC_PATH)
  print(_compute_hash_paths(_SRC_PATH, *deps))


def do_checkout(checkout_path):
  deps = _get_deps(_SRC_PATH)
  # Copy all contents under deps to `<checkout_path>/src`
  for dep in deps:
    dep_pieces = dep.split('/')
    chromium_dep_path = os.path.join(_SRC_PATH, *dep_pieces)
    checkout_dep_path = os.path.join(checkout_path, 'src', *dep_pieces)

    checkout_dep_par_path = os.path.abspath(
        os.path.join(checkout_dep_path, os.path.pardir))
    if not os.path.exists(checkout_dep_par_path):
      os.makedirs(checkout_dep_par_path)

    # Since _BASE_DEPS and deps may have overlaps, continue if a path exists
    if os.path.exists(checkout_dep_path):
      continue

    if os.path.isdir(chromium_dep_path):
      shutil.copytree(chromium_dep_path, checkout_dep_path, symlinks=True)
    else:
      shutil.copy(chromium_dep_path, checkout_dep_path)


def main():
  ap = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=lambda _opts: do_latest())

  checkout = sub.add_parser("checkout")
  checkout.add_argument("checkout_path")
  checkout.set_defaults(func=lambda opts: do_checkout(opts.checkout_path))

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()

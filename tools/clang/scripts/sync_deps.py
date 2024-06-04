#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script syncs the Clang and Rust revisions defined in update.py
and update_rust.py with the deps entries in DEPS."""

import argparse
import hashlib
import re
import os
import subprocess
import sys
import tempfile

from update import DownloadUrl, CDS_URL, CLANG_REVISION

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                 'rust'))
from update_rust import RUST_REVISION, RUST_SUB_REVISION


def GetDepsObjectInfo(object_name: str) -> str:
  url = f'{CDS_URL}/{object_name}'
  describe_url = f'gs://chromium-browser-clang/{object_name}'
  output = subprocess.check_output(['gsutil.py', 'stat',
                                    describe_url]).decode("utf-8")
  # Output looks like:
  # ``
  # gs://bucket/path:
  #     Creation time:          Wed, 15 May 2024 13:36:30 GMT
  #     Update time:            Wed, 15 May 2024 13:36:30 GMT
  #     Storage class:          STANDARD
  #     Cache-Control:          public, max-age=31536000,no-transform
  #     Content-Encoding:       gzip
  #     Content-Length:         5766650
  #     Content-Type:           application/octet-stream
  #     Hash (crc32c):          E8z0Sg==
  #     Hash (md5):             E/XAhJhhpd5+08cdO17CFA==
  #     ETag:                   COvj8aXjj4YDEAE=
  #     Generation:             1715780189975019
  #     Metageneration:         1
  generation = re.search('Generation:\s+([0-9]+)', output).group(1)
  size_bytes = re.search('Content-Length:\s+([0-9]+)', output).group(1)
  with tempfile.NamedTemporaryFile() as f:
    DownloadUrl(url, f)
    f.seek(0)
    sha256sum = hashlib.file_digest(f, 'sha256').hexdigest()

  return f'{object_name},{sha256sum},{size_bytes},{generation}'


def GetRustObjectNames() -> list:
  object_names = []
  for host_os in ['Linux_x64', 'Mac', 'Mac_arm64', 'Win']:
    rust_version = (f'{RUST_REVISION}-{RUST_SUB_REVISION}')
    clang_revision = CLANG_REVISION
    object_name = f'{host_os}/rust-toolchain-{rust_version}-{clang_revision}'
    object_names.append(f'{object_name}.tar.xz')
  return object_names


def main():
  rust_object_infos = [
      GetDepsObjectInfo(o) for o in sorted(GetRustObjectNames())
  ]

  rust_object_infos_string = '?'.join(rust_object_infos)
  rust_deps_entry_path = 'src/third_party/rust-toolchain'
  rust_setdep_string = f'{rust_deps_entry_path}@{rust_object_infos_string}'

  rust_setdep_args = ['gclient', 'setdep', f'--revision={rust_setdep_string}']
  subprocess.run(rust_setdep_args)


if __name__ == '__main__':
  sys.exit(main())

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import re
import struct


def _SanitizeName(name: str) -> str:
  return re.sub('[^0-9a-zA-Z_]', '_', name)


def HashName(name: str) -> int:
  # This must match the hash function in //base/metrics/metrics_hashes.cc.
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(name.encode()).digest()[:8])[0]


class FileInfo(object):
  """A class to hold codegen information about a file."""
  def __init__(self, relpath: str, basename: str) -> None:
    self.dir_path = relpath
    self.guard_path = _SanitizeName(os.path.join(relpath, basename)).upper()


class ModelTypeInfo(object):
  """A class to hold codegen information about a model type such as metric."""
  def __init__(self, json_obj: dict) -> None:
    self.raw_name = json_obj['name']
    self.name = _SanitizeName(json_obj['name'])
    self.hash = HashName(json_obj['name'])


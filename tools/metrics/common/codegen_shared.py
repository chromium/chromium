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


def HashFieldTrialName(field_trial_name: str) -> int:
  """Computes the hash of a field trial or group name.

  Corresponds to HashName() in Chromium's components/variations/hashing.cc,
  and HashFieldTrialName() in Chromium's base/metrics/metrics_hashes.cc.

  Args:
    field_trial_name: The string to hash (a study or a group name).

  Returns:
    The hash of |field_trial_name| as an integer.
  """
  # This must match the hash function in //base/metrics/metrics_hashes.cc.
  # <L: 4 bytes, little endian.
  return struct.unpack(
      '<L',
      hashlib.sha1(field_trial_name.encode('utf-8')).digest()[:4])[0]


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

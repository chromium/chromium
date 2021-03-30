# Lint as: python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import pathlib
import sys
from typing import Dict

import owners_data

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))

sys.path.append(
    os.path.join(_SRC_PATH, 'tools', 'android', 'dependency_analysis'))
import subprocess_utils


def read_raw_dir_metadata(chromium_root: str, dirmd_path: str) -> Dict:
  '''Runs all DIR_METADATA files with dirmd and returns a dict of raw data.'''
  raw_str: str = subprocess_utils.run_command(
      [dirmd_path, 'export', '--root', chromium_root])
  raw_dict: Dict = json.loads(raw_str)
  return raw_dict['dirs']


def build_dir_metadata(chromium_root: str, all_dir_metadata: Dict,
                       requested_path: owners_data.RequestedPath
                       ) -> owners_data.DirMetadata:
  '''Creates a synthetic representation of an DIR_METADATA file.'''
  return _build_dir_metadata_recursive(chromium_root, all_dir_metadata,
                                       pathlib.Path(requested_path.path))


# Memoized synthetic dir_metadatas
synthetic_dir_metadatas: Dict[pathlib.Path, owners_data.DirMetadata] = {}


def _build_dir_metadata_recursive(chromium_root: str, all_dir_metadata: Dict,
                                  path: pathlib.Path
                                  ) -> owners_data.DirMetadata:
  # Use memoized value
  if path in synthetic_dir_metadatas:
    return synthetic_dir_metadatas[path]

  # Clone parent synthetic dir_metadata as base
  if len(path.parts) > 1:
    parent_dir_metadata = _build_dir_metadata_recursive(chromium_root,
                                                        all_dir_metadata,
                                                        path.parent)
    dir_metadata = parent_dir_metadata.copy()
  else:
    dir_metadata = owners_data.DirMetadata()

  # Add data from all_dir_metadata, if there is a DIR_METADATA in the directory
  # Be careful to keep values inherited from the parent dir.
  raw_dict = all_dir_metadata.get(str(path), {})
  monorail = raw_dict.get('monorail', {})
  dir_metadata.component = monorail.get('component', dir_metadata.component)
  dir_metadata.team = raw_dict.get('teamEmail', dir_metadata.team)
  dir_metadata.os = raw_dict.get('os', dir_metadata.os)

  # Memoize and return
  synthetic_dir_metadatas[path] = dir_metadata
  return dir_metadata

# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from typing import List

import owners_data

_IGNORED_FOLDERS = ('out', 'third_party', 'clank', 'build/linux',
                    'native_client')
_KNOWN_FOLDERS = [
    r'^chrome\/browser\/(.*)\/android$', r'^chrome\/browser\/android\/(.*)$',
    r'^chrome\/android\/(.*)$',
    r'^chrome\/android\/java\/src\/org\/chromium\/chrome\/browser\/(.*)$',
    r'^chrome\/android\/features\/(.*)$',
    r'^chrome\/android\/javatests\/src\/org\/chromium\/chrome\/browser\/(.*)$',
    r'^chrome\/android\/native_java_unittests\/src\/org\/chromium\/chrome\/browser\/(.*)$',
    r'^chrome\/android\/junit\/src\/org\/chromium\/chrome\/browser\/(.*)$',
    r'^components\/(.*)\/android$',
    r'^content\/public\/android\/java\/src\/org\/chromium\/content\/browser\/(.*)$'
]


def get_android_folders(chromium_root: str,
                        limit_to_dir: str) -> List[owners_data.RequestedPath]:
  '''Get all directories containing `android/` in their path.

  Use _IGNORED_FOLDERS to exclude commonly returned folders that
  need to be excluded from the resultset. Use _KNOWN_FOLDERS to propose
  feature names to the folders based on their patterns.

  If limit_to_dir is non-empty, only traverse that dir and its subdirectories.
  '''

  android_folders = []
  android_folders_found = set()

  for full_root, dirs, _ in os.walk(chromium_root):
    assert full_root.startswith(chromium_root)
    root = full_root[len(chromium_root) + 1:]
    if root.startswith(_IGNORED_FOLDERS):
      continue
    if limit_to_dir and not root.startswith(limit_to_dir):
      continue

    for name in dirs:
      fullpath = os.path.join(root, name)

      for folder_token in _KNOWN_FOLDERS:
        found = False
        re_search = re.match(folder_token, fullpath, re.IGNORECASE)
        if re_search:
          feature = re_search.group(1)
          if folder_token.endswith('(.*)$'):
            if '/' not in feature:
              android_folders.append(
                  owners_data.RequestedPath(fullpath, feature))
              found = True
          else:
            feature = feature.split('/')[0] if '/' in feature else feature
            android_folders.append(owners_data.RequestedPath(fullpath, feature))
            found = True
        if found:
          android_folders_found.add(fullpath)
          break

      if fullpath.endswith('/android') \
        and fullpath not in android_folders_found:
        feature = fullpath.split('/')[0] if '/' in fullpath\
          and not fullpath.startswith('chrome/') else fullpath
        android_folders.append(owners_data.RequestedPath(fullpath, feature))
        android_folders_found.add(fullpath)

  return android_folders

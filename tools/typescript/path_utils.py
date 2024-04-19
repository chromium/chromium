# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def isInAshFolder(path):
  nested_lacros_folders = [
      'chrome/browser/resources/chromeos/kerberos',
  ]
  if any(path.startswith(folder) for folder in nested_lacros_folders):
    return False

  # TODO (https://crbug.com/1506296): Organize Ash WebUI code under fewer
  # directories.
  ash_folders = [
      # Source code folders
      'ash/webui',
      'chrome/browser/resources/ash',
      'chrome/browser/resources/chromeos',
      'chrome/browser/resources/nearby_internals',
      'chrome/browser/resources/nearby_share',
      'ui/file_manager',

      # Test folders
      'chrome/test/data/webui/chromeos',
      'chrome/test/data/webui/cr_components/chromeos',
      'chrome/test/data/webui/nearby_share',
  ]
  return any(path.startswith(folder) for folder in ash_folders)


def getTargetPath(gen_dir, root_gen_dir):
  root_gen_dir_from_build = os.path.normpath(os.path.join(
      gen_dir, root_gen_dir)).replace('\\', '/')
  return os.path.relpath(gen_dir, root_gen_dir_from_build).replace('\\', '/')

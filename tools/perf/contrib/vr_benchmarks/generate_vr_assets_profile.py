# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Generates a Chrome profile that allows VR asset use in Telemetry tests.

Copies all necessary files into a generated directory and creates a manifest.
"""

import argparse
import json
import os
import shutil
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output',
                      required=True,
                      help='The gen directory to put the profile directory in')
  parser.add_argument('--asset-dir',
                      required=True,
                      help='The directory containing the asset information')
  args, _ = parser.parse_known_args()

  # Add the directory to the path so we can get the parse_version script.
  asset_dir = args.asset_dir
  sys.path.append(asset_dir)
  #pylint: disable=import-error,import-outside-toplevel
  import parse_version

  # Get the assets version.
  with open(os.path.join(asset_dir, 'VERSION'), 'r') as version_file:
    version = parse_version.ParseVersion(version_file.readlines())
  if not version:
    raise RuntimeError('Could not get version for VR assets')

  # Clean up a pre-existing profile and create the necessary directories.
  profile_dir = os.path.join(args.output, 'vr_assets_profile')
  if os.path.isdir(profile_dir):
    shutil.rmtree(profile_dir)
  os.makedirs(profile_dir)

  # Check whether there are actually files to copy - if not, the lack of a
  # directory will cause Telemetry to fail if we actually try to use the profile
  # directory. This is a workaround for not being able to check whether we
  # should have the files in GN. This way, we won't just fail silently during
  # tests, i.e. use the fallback assets, but we don't get build errors on bots
  # due to trying to copy non-existent files.
  found_asset = False
  for asset in os.listdir(os.path.join(asset_dir, 'google_chrome')):
    if asset.endswith('.png') or asset.endswith('.jpeg'):
      found_asset = True
      break
  if not found_asset:
    return
  profile_dir = os.path.join(profile_dir, 'VrAssets',
                             '%d.%d' % (version.major, version.minor))
  os.makedirs(profile_dir)

  # Only copy the files specified by the asset JSON file.
  with open(os.path.join(asset_dir,
        'vr_assets_component_files.json'), 'r') as asset_json_file:
    asset_files = json.load(asset_json_file)
    for asset in asset_files:
      shutil.copy(os.path.join(asset_dir, asset), profile_dir)

  # Generate the manifest file.
  with open(os.path.join(profile_dir, 'manifest.json'), 'w') as manifest_file:
    json.dump({
        'manifest_version': 2,
        'name': 'VrAssets',
        'version': '%d.%d' % (version.major, version.minor)}, manifest_file)

if __name__ == '__main__':
  main()

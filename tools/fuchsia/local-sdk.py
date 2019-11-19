#!/usr/bin/env python

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile


SELF_FILE = os.path.normpath(os.path.abspath(__file__))
REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))


def Run(*args):
  print('Run:', ' '.join(args))
  subprocess.check_call(args)


def EnsureEmptyDir(path):
  if os.path.isdir(path):
    shutil.rmtree(path)
  if not os.path.exists(path):
    print('Creating directory', path)
    os.makedirs(path)


def BuildForArch(arch):
  build_dir = 'out/release-' + arch
  Run('scripts/fx', '--dir', build_dir, 'set', 'terminal.qemu-'+ arch,
      '--with=//topaz/packages/sdk:topaz', '--with-base=//sdk/bundles:tools',
      '--args=is_debug=false', '--args=build_sdk_archives=true')
  Run('scripts/fx', 'build', 'topaz/public/sdk:fuchsia_dart', 'sdk',
      'build/images')


def main(args):
  if len(args) == 0 or len(args) > 2 or not os.path.isdir(args[0]):
    print("""usage: %s <path_to_fuchsia_tree> [architecture]""" % SELF_FILE)
    return 1

  target_archs = []
  if len(args) > 1:
    arch = args[1]
    if arch not in ['x64', 'arm64']:
      print('Unknown architecture: ' + arch)
      print('Must be "x64" or "arm64".')
      return 1
    target_archs = [arch]
  else:
    target_archs = ['x64', 'arm64']

  # Nuke the SDK from DEPS, put our just-built one there, and set a fake .hash
  # file. This means that on next gclient runhooks, we'll restore to the
  # real DEPS-determined SDK.
  sdk_output_dir = os.path.join(REPOSITORY_ROOT, 'third_party', 'fuchsia-sdk',
                            'sdk')
  images_output_dir = os.path.join(REPOSITORY_ROOT, 'third_party',
                                   'fuchsia-sdk', 'images')
  EnsureEmptyDir(sdk_output_dir)
  EnsureEmptyDir(images_output_dir)

  original_dir = os.getcwd()
  fuchsia_root = os.path.abspath(args[0])
  merged_manifest = None
  manifest_parts = set()

  # Switch to the Fuchsia tree and build the SDKs.
  os.chdir(fuchsia_root)

  for arch in target_archs:
    BuildForArch(arch)

    arch_output_dir = os.path.join(fuchsia_root, 'out', 'release-' + arch)
    sdk_tars = [
        os.path.join(arch_output_dir, 'sdk', 'archive', 'core.tar.gz'),
        os.path.join(arch_output_dir, 'sdk', 'archive', 'fuchsia_dart.tar.gz'),
    ]

    # Extract tars merging manifests
    manifest_path = os.path.join(sdk_output_dir, 'meta', 'manifest.json')
    for sdk_tar in sdk_tars:
      with tarfile.open(sdk_tar, mode='r:gz') as tar:
        for tar_file in tar:
          try:
            tar.extract(tar_file, sdk_output_dir)
          except IOError:
            # Ignore overwrite of read-only files.
            pass

      # Merge the manifest ensuring that we don't have duplicate entries.
      if os.path.isfile(manifest_path):
        manifest = json.load(open(manifest_path))
        os.remove(manifest_path)
        if not merged_manifest:
          merged_manifest = manifest
          for part in manifest['parts']:
            manifest_parts.add(part['meta'])
        else:
          for part in manifest['parts']:
            if part['meta'] not in manifest_parts:
              manifest_parts.add(part['meta'])
              merged_manifest['parts'].append(part)

    arch_image_dir = os.path.join(images_output_dir, arch, 'qemu')
    os.mkdir(os.path.join(images_output_dir, arch))
    os.mkdir(arch_image_dir)

    # Stage the image directory using entries specified in the build image
    # manifest.
    images_json = json.load(open(os.path.join(arch_output_dir, 'images.json')))
    for entry in images_json:
      if entry['type'] not in ['blk', 'zbi', 'kernel']:
        continue

      shutil.copyfile(os.path.join(arch_output_dir, entry['path']),
                      os.path.join(arch_image_dir, entry['name']) + '.' +
                          entry['type'])

  # Write merged manifest file.
  with open(manifest_path, 'w') as manifest_file:
    json.dump(merged_manifest, manifest_file, indent=2)

  print('Hashing sysroot...')
  # Hash the sysroot to catch updates to the headers, but don't hash the whole
  # tree, as we want to avoid rebuilding all of Chromium if it's only e.g. the
  # kernel blob has changed. https://crbug.com/793956.
  sysroot_hash_obj = hashlib.sha1()
  for root, dirs, files in os.walk(os.path.join(sdk_output_dir, 'sysroot')):
    for f in files:
      path = os.path.join(root, f)
      sysroot_hash_obj.update(path)
      sysroot_hash_obj.update(open(path, 'rb').read())
  sysroot_hash = sysroot_hash_obj.hexdigest()

  hash_filename = os.path.join(sdk_output_dir, '.hash')
  with open(hash_filename, 'w') as f:
    f.write('locally-built-sdk-' + sysroot_hash)

  # Clean up.
  os.chdir(original_dir)

  subprocess.check_call([os.path.join(REPOSITORY_ROOT, 'third_party',
                                      'fuchsia-sdk',
                                      'gen_build_defs.py')])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import filecmp
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile

SELF_FILE = os.path.normpath(os.path.abspath(__file__))
REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))


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
  Run('scripts/fx', '--dir', 'out/release-{}'.format(arch), 'set',
      'terminal.qemu-{}'.format(arch), '--args=is_debug=false',
      '--args=build_sdk_archives=true')
  Run('scripts/fx', 'build', 'sdk', 'build/images')


def Copy(src, dst):
  if os.path.exists(dst) and filecmp.cmp(src, dst, shallow=False):
    return
  shutil.copy2(src, dst)


def main(args):
  if len(args) == 0 or not os.path.isdir(args[0]):
    print("""usage: %s <path_to_fuchsia_tree> [architecture]""" % SELF_FILE)
    return 1

  ALL_ARCHS = set(['x64', 'arm64'])
  if len(args) == 1:
    target_archs = ALL_ARCHS
  else:
    target_archs = set(args[1:])
    unknown_archs = target_archs - ALL_ARCHS
    if unknown_archs:
      print(
          f'Unknown architectures: {unknown_archs}. Known architectures: {ALL_ARCHS}'
      )
      return 1

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

    sdk_tarballs = ['core.tar.gz', 'core_testing.tar.gz']

    for sdk_tar in sdk_tarballs:
      sdk_tar_path = os.path.join(arch_output_dir, 'sdk', 'archive', sdk_tar)
      sdk_gn_dir = os.path.join(arch_output_dir, 'sdk', 'gn-' + sdk_tar)

      # Process the Core SDK tarball to generate the GN SDK.
      Run('scripts/sdk/gn/generate.py', '--archive', sdk_tar_path, '--output',
          sdk_gn_dir)

      shutil.copytree(sdk_gn_dir,
                      sdk_output_dir,
                      copy_function=Copy,
                      dirs_exist_ok=True)

      # Merge the manifests.
      manifest_path = os.path.join(sdk_output_dir, 'meta', 'manifest.json')
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
      # Not all images are actually built. Only copy images with the 'archive'
      # tag.
      if not entry.get('archive'):
        continue

      shutil.copyfile(
          os.path.join(arch_output_dir, entry['path']),
          os.path.join(arch_image_dir, entry['name']) + '.' + entry['type'])

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

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

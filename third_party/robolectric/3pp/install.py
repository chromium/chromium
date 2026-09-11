#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""3pp install script for Robolectric android-all jars.

For runtime jars (android-all-instrumented-*.jar):
- Extracts platform-specific native runtime libraries (.so on Linux, .dylib on
  macOS) and data (icu/, fonts/, hyphen-data/) to pre-extracted/<jar_name>/.
- Deletes foreign native libraries (.dll, and non-target platform binaries).
- Rewrites jars to lib/<jar_name> with compression disabled (ZIP_STORED).

For compile-time jars (android-all-*.jar without -instrumented):
- Deletes all non-.class files instead of extracting them.
- Rewrites jars to lib/<jar_name> with compression disabled (ZIP_STORED).
"""

import argparse
import glob
import os
import shutil
import sys
import zipfile


_PLATFORM_CONFIG = {
    'linux-amd64': {
        'native_dirs': ('native/linux/x86_64/',),
        'native_exts': ('.so',),
    },
    'mac-arm64': {
        'native_dirs': ('native/mac/aarch64/', 'native/mac/arm64/'),
        'native_exts': ('.dylib',),
    },
}


def _get_target_platform():
  platform = os.environ.get('_3PP_PLATFORM')
  if platform:
    if platform not in _PLATFORM_CONFIG:
      raise ValueError(f'Unsupported _3PP_PLATFORM: {platform}')
    return platform
  if sys.platform == 'darwin':
    return 'mac-arm64'
  return 'linux-amd64'


def _should_remove(name):
  return (
      name.startswith(('native/', 'icu/', 'hyphen-data/', 'fonts/'))
      or name.endswith(('.so', '.dylib', '.dll'))
  )


def _should_extract(name, platform):
  if name.startswith(('icu/', 'hyphen-data/', 'fonts/')):
    return True

  config = _PLATFORM_CONFIG[platform]
  return name.startswith(config['native_dirs']) and name.endswith(
      config['native_exts']
  )


def _process_compile_only_jar(jar_path, output_lib_dir):
  jar_name = os.path.basename(jar_path)
  out_jar = os.path.join(output_lib_dir, jar_name)

  class_count = 0
  deleted_count = 0
  tmp_out_jar = f'{out_jar}.tmp.{os.getpid()}'
  try:
    with zipfile.ZipFile(jar_path, 'r') as in_zip, zipfile.ZipFile(
        tmp_out_jar, 'w'
    ) as out_zip:
      for info in in_zip.infolist():
        if info.is_dir():
          continue
        if info.filename.endswith('.class'):
          data = in_zip.read(info)
          info.compress_type = zipfile.ZIP_STORED
          out_zip.writestr(info, data)
          class_count += 1
        else:
          deleted_count += 1

    os.replace(tmp_out_jar, out_jar)
  finally:
    if os.path.exists(tmp_out_jar):
      try:
        os.remove(tmp_out_jar)
      except OSError:
        pass

  return jar_name, class_count, deleted_count


def _process_runtime_jar(
    jar_path, output_lib_dir, output_extracted_dir, platform
):
  jar_name = os.path.basename(jar_path)
  out_jar = os.path.join(output_lib_dir, jar_name)
  jar_extracted_dir = os.path.join(output_extracted_dir, jar_name)

  if os.path.exists(jar_extracted_dir):
    shutil.rmtree(jar_extracted_dir, ignore_errors=True)

  extracted_count = 0
  deleted_count = 0
  tmp_out_jar = f'{out_jar}.tmp.{os.getpid()}'
  try:
    with zipfile.ZipFile(jar_path, 'r') as in_zip, zipfile.ZipFile(
        tmp_out_jar, 'w'
    ) as out_zip:
      for info in in_zip.infolist():
        if info.is_dir():
          continue

        if _should_extract(info.filename, platform):
          target_path = os.path.join(jar_extracted_dir, info.filename)
          os.makedirs(os.path.dirname(target_path), exist_ok=True)
          with in_zip.open(info) as src, open(target_path, 'wb') as dst:
            shutil.copyfileobj(src, dst)
          if info.filename.endswith(('.so', '.dylib')):
            os.chmod(target_path, 0o755)
          extracted_count += 1

        if _should_remove(info.filename):
          deleted_count += 1
          continue

        data = in_zip.read(info)
        info.compress_type = zipfile.ZIP_STORED
        out_zip.writestr(info, data)

    os.replace(tmp_out_jar, out_jar)
  finally:
    if os.path.exists(tmp_out_jar):
      try:
        os.remove(tmp_out_jar)
      except OSError:
        pass

  return jar_name, extracted_count, deleted_count


def _find_jars(input_dir, output_prefix):
  candidates = []
  if input_dir:
    candidates.extend([os.path.join(input_dir, 'lib'), input_dir])
  else:
    candidates.extend([
        os.path.join(os.getcwd(), 'lib'),
        os.getcwd(),
        os.path.join(output_prefix, 'lib'),
        output_prefix,
    ])
  for d in candidates:
    if os.path.isdir(d):
      jars = sorted(glob.glob(os.path.join(d, '*.jar')))
      if jars:
        print(f'Using .jars from {d}')
        return jars, d
  raise RuntimeError(f'No .jar files found in candidates: {candidates}')


def _install(output_prefix, input_dir=None):
  platform = _get_target_platform()
  jars, source_dir = _find_jars(input_dir, output_prefix)

  output_lib_dir = os.path.join(output_prefix, 'lib')
  output_extracted_dir = os.path.join(output_prefix, 'pre-extracted')
  os.makedirs(output_lib_dir, exist_ok=True)

  print(
      f'Processing {len(jars)} jars for {platform} '
      f'from {source_dir} -> {output_prefix}...'
  )
  for i, jar_path in enumerate(jars, 1):
    jar_name = os.path.basename(jar_path)
    print(f'[{i}/{len(jars)}] Processing {jar_name}...')
    if '-instrumented' not in jar_name:
      _, classes, deleted = _process_compile_only_jar(
          jar_path, output_lib_dir
      )
      print(
          f'  Deleted {deleted} non-.class files, kept {classes} .class '
          f'files (compile-time jar)'
      )
    else:
      _, extracted, deleted = _process_runtime_jar(
          jar_path, output_lib_dir, output_extracted_dir, platform
      )
      print(
          f'  Extracted {extracted} files, deleted {deleted} non-{platform} '
          f'files, saved uncompressed jar'
      )


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('output_prefix', help='Path to output directory.')
  parser.add_argument(
      'deps_prefix', nargs='?', help='Path to deps directory (unused).'
  )
  parser.add_argument(
      '--input-dir', help='Path to directory containing lib/*.jar.'
  )
  args = parser.parse_args()

  _install(args.output_prefix, input_dir=args.input_dir)


if __name__ == '__main__':
  main()

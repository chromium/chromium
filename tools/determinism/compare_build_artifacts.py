#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compare the artifacts from two builds."""

from __future__ import print_function

import ast
import difflib
import glob
import json
import optparse
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def get_files_to_compare(build_dir, recursive=False):
  """Get the list of files to compare."""
  allowed = frozenset((
    '.aab',
    '.apk',
    '.apks',
    '.app',
    '.bin',  # V8 snapshot file snapshot_blob.bin
    '.dll',
    '.dylib',
    '.exe',
    '.isolated',
    '.nexe',
    '.pdb',
    '.so',
    '.zip',
  ))
  def check(f):
    if not os.path.isfile(f):
      return False
    if os.path.basename(f).startswith('.'):
      return False
    ext = os.path.splitext(f)[1]
    if ext in allowed:
      return True
    # Special case for file without an extension that has the executable bit
    # set.
    return ext == '' and os.access(f, os.X_OK)

  ret_files = set()
  for root, dirs, files in os.walk(build_dir):
    if not recursive:
      dirs[:] = [d for d in dirs if d.endswith('_apk')]
    for f in (f for f in files if check(os.path.join(root, f))):
      ret_files.add(os.path.relpath(os.path.join(root, f), build_dir))
  return ret_files


def get_files_to_compare_using_isolate(build_dir):
  # First, find all .isolate files in build_dir.
  # TODO(maruel): https://crbug.com/972075 Extract targets from mb.
  isolates = glob.glob(os.path.join(build_dir, '*.isolate'))

  # Then, extract their contents.
  ret_files = set()

  for isolate in isolates:
    with open(isolate) as f:
      isolate_contents = ast.literal_eval(f.read())
      isolate_files = isolate_contents['variables']['files']
      for isolate_file in isolate_files:
        normalized_path = os.path.normpath(
            os.path.join(build_dir, isolate_file))

        # Ignore isolate files that are not in the build dir ... for now.
        # If we ever move to comparing determinism of artifacts built from two
        # repositories, we'll want to get rid of this check.
        if os.path.commonprefix((normalized_path, build_dir)) != build_dir:
          continue

        # Theoretically, directories should end in '/' and files should not, but
        # this doesn't hold true because some GN build files are incorrectly
        # configured. We explicitly check whether the path is a directory or
        # file.
        if not os.path.isdir(normalized_path):
          ret_files.add(normalized_path)
          continue

        for root, dirs, files in os.walk(normalized_path):
          for inner_file in files:
            ret_files.add(os.path.join(root, inner_file))

  # Convert back to a relpath since that's what the caller is expecting.
  return set(os.path.relpath(f, build_dir) for f in ret_files)


def diff_binary(first_filepath, second_filepath, file_len):
  """Returns a compact binary diff if the diff is small enough."""
  BLOCK_SIZE = 8192
  CHUNK_SIZE = 32
  NUM_CHUNKS_IN_BLOCK = BLOCK_SIZE / CHUNK_SIZE
  MAX_STREAMS = 10
  num_diffs = 0
  streams = []
  offset = 0
  with open(first_filepath, 'rb') as lhs, open(second_filepath, 'rb') as rhs:
    while True:
      lhs_data = lhs.read(BLOCK_SIZE)
      rhs_data = rhs.read(BLOCK_SIZE)
      if not lhs_data or not rhs_data:
        break
      if lhs_data != rhs_data:
        for i in xrange(min(len(lhs_data), len(rhs_data))):
          if lhs_data[i] != rhs_data[i]:
            num_diffs += 1
        if len(streams) < MAX_STREAMS:
          for idx in xrange(NUM_CHUNKS_IN_BLOCK):
            lhs_chunk = lhs_data[idx * CHUNK_SIZE:(idx + 1) * CHUNK_SIZE]
            rhs_chunk = rhs_data[idx * CHUNK_SIZE:(idx + 1) * CHUNK_SIZE]
            if lhs_chunk != rhs_chunk:
              if len(streams) < MAX_STREAMS:
                streams.append((offset + CHUNK_SIZE * idx,
                                lhs_chunk, rhs_chunk))
              else:
                break
      offset += len(lhs_data)
      del lhs_data
      del rhs_data
  if not num_diffs:
    return None
  result = '%d out of %d bytes are different (%.2f%%)' % (
        num_diffs, file_len, 100.0 * num_diffs / file_len)
  if streams:
    encode = lambda text: ''.join(i if 31 < ord(i) < 127 else '.' for i in text)
    for offset, lhs_data, rhs_data in streams:
      lhs_line = '%s \'%s\'' % (lhs_data.encode('hex'), encode(lhs_data))
      rhs_line = '%s \'%s\'' % (rhs_data.encode('hex'), encode(rhs_data))
      diff = list(difflib.Differ().compare([lhs_line], [rhs_line]))[-1][2:-1]
      result += '\n  0x%-8x: %s\n              %s\n              %s' % (
            offset, lhs_line, rhs_line, diff)
  return result


def diff_zips(first_filepath, second_filepath):
  with zipfile.ZipFile(first_filepath) as z1, \
       zipfile.ZipFile(second_filepath) as z2:
    names1 = z1.namelist()
    names2 = z2.namelist()
    # This kind of difference is rare, so don't put effort into printing the
    # exact difference.
    if names1 != names2:
      diff = sorted(set(names1).symmetric_difference(names2))
      if diff:
        return '  Zip file lists differ:\n' + '\n'.join(
            '    ' + f for f in diff)
      else:
        return '  Zips contain same files, but in different orders.'

    diffs = []
    for info1 in z1.infolist():
      info2 = z2.getinfo(info1.filename)
      # Check for the two most common errors. The binary diff will still run
      # to check the rest.
      if info1.CRC != info2.CRC:
        diffs.append('  {}: CRCs differ'.format(info1.filename))
      if info1.date_time != info2.date_time:
        diffs.append('  {}: Timestamps differ'.format(info1.filename))
    # Don't be too spammy.
    if len(diffs) > 5:
      diffs[5:] = ['   ...']
  return '\n'.join(diffs)


def memoize(f):
  memo = {}
  def helper(*args):
    if args not in memo:
      memo[args] = f(*args)
    return memo[args]
  return helper


# compare_deps() can be called with different targets that all depend on
# "all" targets, so memoize the results of this function to make sure we
# don't compare "all" files more than once.
@memoize
def compare_files(first_filepath, second_filepath):
  """Compares two binaries and return the number of differences between them.

  Returns None if the files are equal, a string otherwise.
  """
  if not os.path.exists(first_filepath):
    return 'file does not exist %s' % first_filepath
  if not os.path.exists(second_filepath):
    return 'file does not exist %s' % second_filepath

  ret = None
  file_len = os.stat(first_filepath).st_size
  if file_len != os.stat(second_filepath).st_size:
    ret = 'different size: %d != %d' % (file_len,
                                        os.stat(second_filepath).st_size)
  else:
    ret = diff_binary(first_filepath, second_filepath, file_len)

  if ret and zipfile.is_zipfile(first_filepath) and zipfile.is_zipfile(
      second_filepath):
    ret += '\n' + diff_zips(first_filepath, second_filepath)
  return ret


def get_deps(ninja_path, build_dir, target):
  """Returns list of object files needed to build target."""
  NODE_PATTERN = re.compile(r'label="([a-zA-Z0-9_\\/.-]+)"')
  CHECK_EXTS = ('.o', '.obj')

  # Rename to possibly original directory name if possible.
  fixed_build_dir = build_dir
  if build_dir.endswith('.1') or build_dir.endswith('.2'):
    fixed_build_dir = build_dir[:-2]
    if os.path.exists(fixed_build_dir):
      print(
          'fixed_build_dir %s exists.'
          ' will try to use orig dir.' % fixed_build_dir,
          file=sys.stderr)
      fixed_build_dir = build_dir
    else:
      shutil.move(build_dir, fixed_build_dir)

  try:
    out = subprocess.check_output([ninja_path, '-C', fixed_build_dir,
                                   '-t', 'graph', target])
  except subprocess.CalledProcessError as e:
    print('error to get graph for %s: %s' % (target, e), file=sys.stderr)
    return []

  finally:
    # Rename again if we renamed before.
    if fixed_build_dir != build_dir:
      shutil.move(fixed_build_dir, build_dir)

  files = []
  for line in out.splitlines():
    matched = NODE_PATTERN.search(line)
    if matched:
      path = matched.group(1)
      if not os.path.splitext(path)[1] in CHECK_EXTS:
        continue
      if os.path.isabs(path):
        print(
            'not support abs path %s used for target %s' % (path, target),
            file=sys.stderr)
        continue
      files.append(path)
  return files


def compare_deps(first_dir, second_dir, ninja_path, targets):
  """Print difference of dependent files."""
  diffs = set()
  for target in targets:
    first_deps = get_deps(ninja_path, first_dir, target)
    second_deps = get_deps(ninja_path, second_dir, target)
    print('Checking %s difference: (%s deps)' % (target, len(first_deps)))
    if set(first_deps) != set(second_deps):
      # Since we do not thiks this case occur, we do not do anything special
      # for this case.
      print('deps on %s are different: %s' %
            (target, set(first_deps).symmetric_difference(set(second_deps))))
      continue
    max_filepath_len = max([0] + [len(n) for n in first_deps])
    for d in first_deps:
      first_file = os.path.join(first_dir, d)
      second_file = os.path.join(second_dir, d)
      result = compare_files(first_file, second_file)
      if result:
        print('  %-*s: %s' % (max_filepath_len, d, result))
        diffs.add(d)
  return list(diffs)


def compare_build_artifacts(first_dir, second_dir, ninja_path, target_platform,
                            json_output, recursive, use_isolate_files):
  """Compares the artifacts from two distinct builds."""
  if not os.path.isdir(first_dir):
    print('%s isn\'t a valid directory.' % first_dir, file=sys.stderr)
    return 1
  if not os.path.isdir(second_dir):
    print('%s isn\'t a valid directory.' % second_dir, file=sys.stderr)
    return 1

  epoch_hex = struct.pack('<I', int(time.time())).encode('hex')
  print('Epoch: %s' %
      ' '.join(epoch_hex[i:i+2] for i in xrange(0, len(epoch_hex), 2)))

  with open(os.path.join(BASE_DIR, 'deterministic_build_whitelist.pyl')) as f:
    raw_whitelist = ast.literal_eval(f.read())
    whitelist_list = raw_whitelist[target_platform]
    if re.search(r'\bis_component_build\s*=\s*true\b',
                 open(os.path.join(first_dir, 'args.gn')).read()):
      whitelist_list += raw_whitelist.get(target_platform + '_component', [])
    whitelist = frozenset(whitelist_list)

  if use_isolate_files:
    first_list = get_files_to_compare_using_isolate(first_dir)
    second_list = get_files_to_compare_using_isolate(second_dir)
  else:
    first_list = get_files_to_compare(first_dir, recursive)
    second_list = get_files_to_compare(second_dir, recursive)


  equals = []
  expected_diffs = []
  unexpected_diffs = []
  unexpected_equals = []
  all_files = sorted(first_list & second_list)
  missing_files = sorted(first_list.symmetric_difference(second_list))
  if missing_files:
    print('Different list of files in both directories:', file=sys.stderr)
    print('\n'.join('  ' + i for i in missing_files), file=sys.stderr)
    unexpected_diffs.extend(missing_files)

  max_filepath_len = 0
  if all_files:
    max_filepath_len = max(len(n) for n in all_files)
  for f in all_files:
    first_file = os.path.join(first_dir, f)
    second_file = os.path.join(second_dir, f)
    result = compare_files(first_file, second_file)
    if not result:
      tag = 'equal'
      equals.append(f)
      if f in whitelist:
        unexpected_equals.append(f)
    else:
      if f in whitelist:
        expected_diffs.append(f)
        tag = 'expected'
      else:
        unexpected_diffs.append(f)
        tag = 'unexpected'
      result = 'DIFFERENT (%s): %s' % (tag, result)
    print('%-*s: %s' % (max_filepath_len, f, result))
  unexpected_diffs.sort()

  print('Equals:           %d' % len(equals))
  print('Expected diffs:   %d' % len(expected_diffs))
  print('Unexpected diffs: %d' % len(unexpected_diffs))
  if unexpected_diffs:
    print('Unexpected files with diffs:\n')
    for u in unexpected_diffs:
      print('  %s' % u)
  if unexpected_equals:
    print('Unexpected files with no diffs:\n')
    for u in unexpected_equals:
      print('  %s' % u)

  all_diffs = expected_diffs + unexpected_diffs
  diffs_to_investigate = sorted(set(all_diffs).difference(missing_files))
  deps_diff = compare_deps(first_dir, second_dir,
                           ninja_path, diffs_to_investigate)

  if json_output:
    try:
      out = {
          'expected_diffs': expected_diffs,
          'unexpected_diffs': unexpected_diffs,
          'deps_diff': deps_diff,
      }
      with open(json_output, 'w') as f:
        json.dump(out, f)
    except Exception as e:
      print('failed to write json output: %s' % e)

  return int(bool(unexpected_diffs))


def main():
  parser = optparse.OptionParser(usage='%prog [options]')
  parser.add_option(
      '-f', '--first-build-dir', help='The first build directory.')
  parser.add_option(
      '-s', '--second-build-dir', help='The second build directory.')
  parser.add_option('-r', '--recursive', action='store_true', default=False,
                    help='Indicates if the comparison should be recursive.')
  parser.add_option(
      '--use-isolate-files', action='store_true', default=False,
      help='Use .isolate files in each directory to determine which artifacts '
           'to compare.')

  parser.add_option('--json-output', help='JSON file to output differences')
  parser.add_option('--ninja-path', help='path to ninja command.',
                    default='ninja')
  target = {
      'darwin': 'mac', 'linux2': 'linux', 'win32': 'win'
  }.get(sys.platform, sys.platform)
  parser.add_option(
      '-t', '--target-platform', help='The target platform.',
      default=target, choices=('android', 'fuchsia', 'mac', 'linux', 'win'))
  options, _ = parser.parse_args()

  if not options.first_build_dir:
    parser.error('--first-build-dir is required')
  if not options.second_build_dir:
    parser.error('--second-build-dir is required')
  if not options.target_platform:
    parser.error('--target-platform is required')

  return compare_build_artifacts(os.path.abspath(options.first_build_dir),
                                 os.path.abspath(options.second_build_dir),
                                 options.ninja_path,
                                 options.target_platform,
                                 options.json_output,
                                 options.recursive,
                                 options.use_isolate_files)


if __name__ == '__main__':
  sys.exit(main())

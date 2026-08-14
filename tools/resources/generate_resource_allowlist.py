#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """generate_resource_allowlist.py [-o OUTPUT] [--depfile DEPFILE] INPUTS...

INPUTS are paths to unstripped binaries or linker input file lists containing
references to resources in their debug info.

This script generates a resource allowlist by reading debug info from
INPUTS and writes it to OUTPUT.
"""

# Allowlisted resources are identified by searching the input file for
# instantiations of the special function ui::AllowlistedResource (see
# ui/base/resource/allowlist.h).

import argparse
import os
import sys

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
import action_helpers
import ar


def _DecodeMsvcInteger(s):
  """Decodes an MSVC-mangled positive integer template argument.

  MSVC encodes positive integer arguments as '0' followed by hex digits
  mapped to letters 'A' (0x0) through 'P' (0xF), terminated by '@'.
  For example, '0BJH@' -> 0x197 = 407.
  """
  if not s or s[0] != ord('0'):
    return None, 0
  val = 0
  for i, c in enumerate(s[1:], 1):
    if c == ord('@'):
      return val, i + 1
    if ord('A') <= c <= ord('P'):
      val = (val << 4) | (c - ord('A'))
    else:
      return None, 0
  return None, 0


def _ExtractAllowlistMsvc(data, resource_ids):
  """Extracts IDs from MSVC-mangled instantiations of
  `ui::AllowlistedResource<ID>()`.

  The mangled form looks like`??$AllowlistedResource@$0<HEX_ID>@@ui@@YAXXZ`,
  where HEX_ID is encoded as in `_DecodeMsvcInteger`.
  """
  prefix = b'??$AllowlistedResource@$'
  start_idx = 0
  while start_idx != -1:
    start_idx = data.find(prefix, start_idx)
    if start_idx != -1:
      start_idx += len(prefix)
      val, advance = _DecodeMsvcInteger(data[start_idx:start_idx + 16])
      if val is not None:
        resource_ids.add(val)
        start_idx += advance


def _ExtractAllowlistItanium(data, resource_ids):
  """Extracts IDs from Itanium-mangled instantiations of
  `ui::AllowlistedResource<ID>()`.

  Matches symbols of the form `_ZN2ui19AllowlistedResourceILi<ID>EEEvv`.
  """
  prefix = b'AllowlistedResourceILi'
  start_idx = 0
  while start_idx != -1:
    start_idx = data.find(prefix, start_idx)
    if start_idx != -1:
      end_idx = data.find(b'E', start_idx)
      resource_ids.add(int(data[start_idx + len(prefix):end_idx]))
      start_idx = end_idx


def GetResourceAllowlistELF(path):
  """Produce a resource allowlist by searching for debug info referring to
  AllowlistedResource.
  """
  # This used to use "readelf -p .debug_str", but it doesn't seem to work with
  # use_debug_fission=true. Reading the raw file is faster anyways.
  resource_ids = set()
  with open(path, 'rb') as f:
    data = f.read()
  _ExtractAllowlistItanium(data, resource_ids)
  return resource_ids, []


def GetResourceAllowlistFileList(file_list_path):
  """Given the list of linker input files:
  1. Scan each object file (including those recursively listed in e.g. a .lib)
     and extract their resource IDs.
  2. Return a list of all files that were examined (including archives) to
     generate deps for GN.
  """
  with open(file_list_path, encoding='utf-8') as f:
    paths = [p for p in f.read().splitlines() if os.path.exists(p)]

  expanded_paths = ar.ExpandThinArchives(paths)

  resource_ids = set()
  for p in expanded_paths:
    if p.endswith('.obj'):
      with open(p, 'rb') as f:
        _ExtractAllowlistMsvc(f.read(), resource_ids)
    elif p.endswith('.o'):
      with open(p, 'rb') as f:
        _ExtractAllowlistItanium(f.read(), resource_ids)
  return resource_ids, set(paths) | set(expanded_paths)


def WriteResourceAllowlist(args):
  resource_ids = set()
  deps = set()
  for input_path in args.inputs:
    with open(input_path, 'rb') as f:
      magic = f.read(4)
      chunk = f.read(60)
    if magic == b'\x7fELF':
      func = GetResourceAllowlistELF
    elif magic.startswith(b'MZ'):
      raise ValueError('Expected linker input file list, got PE binary: %s' %
                       input_path)
    elif (magic == b'obj/' or b'/obj/' in chunk or b'\\obj\\' in chunk
          or input_path.endswith('.dll') or input_path.endswith('.so')):
      # For secondary toolchain, path will look like android_clang_arm/obj/...
      func = GetResourceAllowlistFileList
    else:
      raise ValueError('unknown file format for %s' % input_path)

    cur_resource_ids, cur_deps = func(input_path)
    resource_ids.update(cur_resource_ids)
    deps.update(cur_deps)

  # The last time this broke, exactly two resources were still being found.
  if len(resource_ids) < 100:
    raise RuntimeError('Suspiciously few resources found. Likely an issue with '
                       'the regular expression in this script. Found: ' +
                       ','.join(str(x) for x in sorted(resource_ids)))

  output_content = ''.join(f'{resource_id}\n'
                           for resource_id in sorted(resource_ids))
  if args.output:
    with action_helpers.atomic_output(args.output, mode='w',
                                      encoding='utf-8') as f:
      f.write(output_content)
  else:
    sys.stdout.write(output_content)

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.output, deps)


def main():
  parser = argparse.ArgumentParser(usage=__doc__)
  parser.add_argument('inputs',
                      nargs='+',
                      help='An unstripped binary or linker input file list.')
  parser.add_argument('-o',
                      '--output',
                      help='The resource list path to write (default stdout)')
  action_helpers.add_depfile_arg(parser)

  args = parser.parse_args()
  if args.depfile and not args.output:
    parser.error('--depfile requires -o/--output')

  WriteResourceAllowlist(args)


if __name__ == '__main__':
  main()

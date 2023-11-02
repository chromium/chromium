#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """generate_resource_allowlist.py [-o OUTPUT] INPUTS...

INPUTS are paths to unstripped binaries or PDBs containing references to
resources in their debug info.

This script generates a resource allowlist by reading debug info from
INPUTS and writes it to OUTPUT.
"""

# Allowlisted resources are identified by searching the input file for
# instantiations of the special function ui::AllowlistedResource (see
# ui/base/resource/allowlist.h).

import argparse
import os
import subprocess
import sys

import ar

llvm_bindir = os.path.join(os.path.dirname(sys.argv[0]), '..', '..',
                           'third_party', 'llvm-build', 'Release+Asserts',
                           'bin')


def ExtractAllowlistFromFile(path, resource_ids):
  with open(path, 'rb') as f:
    data = f.read()
  # When symbol_level=0, only mangled names exist.
  # E.g.: _ZN2ui19AllowlistedResourceILi22870EEEvv
  prefix = b'AllowlistedResourceILi'
  start_idx = 0
  while start_idx != -1:
    start_idx = data.find(prefix, start_idx)
    if start_idx != -1:
      end_idx = data.find(b'E', start_idx)
      resource_ids.add(int(data[start_idx + len(prefix):end_idx]))
      start_idx = end_idx


def GetResourceAllowlistELF(path):
  # Produce a resource allowlist by searching for debug info referring to
  # AllowlistedResource.
  # This used to use "readelf -p .debug_str", but it doesn't seem to work with
  # use_debug_fission=true. Reading the raw file is faster anyways.
  resource_ids = set()
  ExtractAllowlistFromFile(path, resource_ids)
  return resource_ids


def GetResourceAllowlistPDB(path):
  # Produce a resource allowlist by using llvm-pdbutil to read a PDB file's
  # publics stream, which is essentially a symbol table, and searching for
  # instantiations of AllowlistedResource. Any such instantiations are demangled
  # to extract the resource identifier.
  pdbutil = subprocess.Popen(
      [os.path.join(llvm_bindir, 'llvm-pdbutil'), 'dump', '-publics', path],
      stdout=subprocess.PIPE)
  names = ''
  for line in pdbutil.stdout:
    line = line.decode('utf8')
    # Read a line of the form
    # "733352 | S_PUB32 [size = 56] `??$AllowlistedResource@$0BFGM@@ui@@YAXXZ`".
    if '`' not in line:
      continue
    sym_name = line[line.find('`') + 1:line.rfind('`')]
    # Under certain conditions such as the GN arg `use_clang_coverage = true` it
    # is possible for the compiler to emit additional symbols that do not match
    # the standard mangled-name format.
    # Example: __profd_??$AllowlistedResource@$0BGPH@@ui@@YAXXZ
    # C++ mangled names are supposed to begin with `?`, so check for that.
    if 'AllowlistedResource' in sym_name and sym_name.startswith('?'):
      names += sym_name + '\n'
  exit_code = pdbutil.wait()
  if exit_code != 0:
    raise Exception('llvm-pdbutil exited with exit code %d' % exit_code)

  undname = subprocess.Popen([os.path.join(llvm_bindir, 'llvm-undname')],
                             stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE)
  stdout, _ = undname.communicate(names.encode('utf8'))
  resource_ids = set()
  for line in stdout.split(b'\n'):
    line = line.decode('utf8')
    # Read a line of the form
    # "void __cdecl ui::AllowlistedResource<5484>(void)".
    prefix = ' ui::AllowlistedResource<'
    pos = line.find(prefix)
    if pos == -1:
      continue
    try:
      resource_ids.add(int(line[pos + len(prefix):line.rfind('>')]))
    except ValueError:
      continue
  exit_code = undname.wait()
  if exit_code != 0:
    raise Exception('llvm-undname exited with exit code %d' % exit_code)
  return resource_ids


def GetResourceAllowlistFileList(file_list_path):
  # Creates a list of resources given the list of linker input files.
  # Simply grep's them for AllowlistedResource<...>.
  with open(file_list_path) as f:
    paths = f.read().splitlines()

  paths = ar.ExpandThinArchives(paths)

  resource_ids = set()
  for p in paths:
    ExtractAllowlistFromFile(p, resource_ids)
  return resource_ids


def WriteResourceAllowlist(args):
  resource_ids = set()
  for input in args.inputs:
    with open(input, 'rb') as f:
      magic = f.read(4)
      chunk = f.read(60)
    if magic == b'\x7fELF':
      func = GetResourceAllowlistELF
    elif magic == b'Micr':
      func = GetResourceAllowlistPDB
    elif magic == b'obj/' or b'/obj/' in chunk:
      # For secondary toolchain, path will look like android_clang_arm/obj/...
      func = GetResourceAllowlistFileList
    else:
      raise Exception('unknown file format')

    resource_ids.update(func(input))

  # The last time this broke, exactly two resources were still being found.
  if len(resource_ids) < 100:
    raise Exception('Suspiciously few resources found. Likely an issue with '
                    'the regular expression in this script. Found: ' +
                    ','.join(sorted(resource_ids)))
  for id in sorted(resource_ids):
    args.output.write(str(id) + '\n')


def main():
  parser = argparse.ArgumentParser(usage=__doc__)
  parser.add_argument('inputs', nargs='+', help='An unstripped binary or PDB.')
  parser.add_argument('-o',
                      dest='output',
                      type=argparse.FileType('w'),
                      default=sys.stdout,
                      help='The resource list path to write (default stdout)')

  args = parser.parse_args()
  WriteResourceAllowlist(args)


if __name__ == '__main__':
  main()

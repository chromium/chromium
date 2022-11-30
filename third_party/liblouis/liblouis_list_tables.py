#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

import json
import optparse

# Matches the include statement in the braille table files.
INCLUDE_RE = re.compile(r"^\s*include\s+([^#\s]+)")

# Exclude these files from table validation.
IGNORE_TABLES = set()
IGNORE_TABLES.add('IPA.utb')
IGNORE_TABLES.add('bh.ctb')
IGNORE_TABLES.add('bo.ctb')
IGNORE_TABLES.add('boxes.ctb')
IGNORE_TABLES.add('de-chess.ctb')
IGNORE_TABLES.add('dra.ctb')
IGNORE_TABLES.add('en-chess.ctb')
IGNORE_TABLES.add('ethio-g1.ctb')
IGNORE_TABLES.add('gon.ctb')
IGNORE_TABLES.add('kok.ctb')
IGNORE_TABLES.add('kru.ctb')
IGNORE_TABLES.add('mun.ctb')
IGNORE_TABLES.add('mwr.ctb')
IGNORE_TABLES.add('or-in-g1.utb')
IGNORE_TABLES.add('pi.ctb')
IGNORE_TABLES.add('spaces.ctb')
IGNORE_TABLES.add('unicode-braille.utb')

def Error(msg):
  sys.stderr.write('liblouis_list_tables: %s' % msg)
  sys.exit(1)


def ToNativePath(pathname):
  return os.path.sep.join(pathname.split('/'))


def LoadTablesFile(filename):
  with open(ToNativePath(filename), 'r') as fh:
    try:
      return json.load(fh)
    except ValueError as e:
      raise ValueError('Error parsing braille table file %s: %s' %
                       (filename, e.message))


def FindFile(filename, directories):
  for d in directories:
    fullname = '/'.join([d, filename])
    if os.path.isfile(ToNativePath(fullname)):
      return fullname
  Error('File not found: %s' % filename)


def FindAllTableFiles(directory):
  ret = set()
  for filename in os.listdir(directory):
    if filename.endswith(".ctb") or filename.endswith(".utb"):
      ret.add(filename)
  return ret


def GetIncludeFiles(filename):
  result = []
  with open(ToNativePath(filename), 'r') as fh:
    for line in fh.readlines():
      match = INCLUDE_RE.match(line)
      if match:
        result.append(match.group(1))
  return result


def GetAdditionalFileTableData(filename):
  result = {}
  INCLUDE_RE = re.compile(r"^#[\+-](\S+):\s*(.+)")
  with open(ToNativePath(filename), 'r') as fh:
    for line in fh.readlines():
      match = INCLUDE_RE.match(line)
      if match:
        result[match.groups(1)[0]] = match.groups(1)[1]
  return result


def ProcessFile(output_set, filename, directories, indent=0):
  fullname = FindFile(filename, directories)
  if fullname in output_set:
    return
  output_set.add(indent*" " + fullname)
  for include_file in GetIncludeFiles(fullname):
    ProcessFile(output_set, include_file, directories, indent=2)


def GetTableFiles(tables_file, directories, extra_files):
  tables = LoadTablesFile(tables_file)
  output_set = set()
  for table in tables:
    for name in table['fileNames'].split(','):
      ProcessFile(output_set, name, directories)
  for name in extra_files:
    ProcessFile(output_set, name, directories)
  return output_set


def CheckTables(tables_file):
  tables = LoadTablesFile(tables_file)
  actual_set = set()
  for table in tables:
    for name in table['fileNames'].split(','):
      actual_set.add(name)
  expected_set = FindAllTableFiles("src/tables")
  errors = []
  for table in actual_set:
    if table not in expected_set and table not in IGNORE_TABLES:
      errors.append("Error: obsolete table not in liblouis " + table)

  new_tables = []
  for table in expected_set:
    if table not in actual_set and table not in IGNORE_TABLES:
      new_tables.append(table)
  return (errors, new_tables)

def DoMain(argv):
  "Entry point for gyp's pymod_do_main command."
  parser = optparse.OptionParser()
  # Give a clearer error message when this is used as a module.
  parser.prog = 'liblouis_list_tables'
  parser.set_usage('usage: %prog [options] listfile')
  parser.add_option('-D', '--directory', dest='directories',
                     action='append', help='Where to search for table files')
  parser.add_option('-e', '--extra_file', dest='extra_files', action='append',
                    default=[], help='Extra liblouis table file to process')
  (options, args) = parser.parse_args(argv)

  if len(args) != 1:
    parser.error('Expecting exactly one argument')
  if not options.directories:
    parser.error('At least one --directory option must be specified')
  files = GetTableFiles(args[0], options.directories, options.extra_files)
  return '\n'.join(files)


def main(argv):
  print(DoMain(argv[1:]))


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    Error('interrupted')

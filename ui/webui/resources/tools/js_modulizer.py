# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates JS files that use native JS Modules from existing JS files that
# either use cr.define() or add to the global namespace directly (like
# assert.js).
#
# This is useful for avoiding code duplication while migration to JS Modules is
# in progress. The eventual goal is to migrate all code to JS modules, at which
# point auto-generation will be unnecessary, and this script can be deleted,
# and the generated code should become the canonical checked-in code.
#
# In order to auto-generate the JS modules, metadata is added to the original
# file in the form of JS comments, like "#export", "#import" and
# "#cr_define_end". See examples in js_modulizer_test.py.
#
# Variables:
#   input_files:
#     The input JS files to be processed.
#
#   in_folder:
#     The folder where |input_files| reside.
#
#   out_folder:
#     The output folder for the generated JS files. Each generated file has the
#     '.m.js' suffix, e.g. assert.js -> assert.m.js.

#   namespace_rewrites:
#     A list of string replacements for replacing global namespaced references
#     with explicitly imported dependencies in the generated JS module.
#     For example "cr.foo.Bar|Bar" will replace all occurrences of "cr.foo.Bar"
#     with "Bar". This flag works identically with the one in
#     polymer_modulizer.gni.

import argparse
import io
import os
import re
import sys

_CWD = os.getcwd()

IMPORT_LINE_REGEX = '// #import'
EXPORT_LINE_REGEX = '/* #export */'
IGNORE_LINE_REGEX = '\s*/\* #ignore \*/(\S|\s)*'

# Ignore lines that contain <include> tags, (for example see util.js).
INCLUDE_LINE_REGEX = '^// <include '

# Ignore this line.
CR_DEFINE_START_REGEX = 'cr.define\('

# Ignore all lines after this comment.
CR_DEFINE_END_REGEX = r'\s+// #cr_define_end'


# Replace various global references with their non-namespaced version, for
# example "cr.ui.Foo" becomes "Foo".
def _rewrite_namespaces(string, namespace_rewrites):
  for rewrite in namespace_rewrites:
    string = string.replace(rewrite, namespace_rewrites[rewrite])
  return string


def ProcessFile(filename, out_folder, namespace_rewrites):
  # Gather indices of lines to be removed.
  indices_to_remove = [];
  renames = {}

  with open(filename) as f:
    lines = f.readlines()
    ignore_remaining_lines = False
    cr_define_start_index = -1
    cr_define_end_index = -1

    for i, line in enumerate(lines):
      if ignore_remaining_lines:
        indices_to_remove.append(i)
        continue

      if re.match(INCLUDE_LINE_REGEX, line):
        indices_to_remove.append(i)
        continue

      if re.match(IGNORE_LINE_REGEX, line):
        indices_to_remove.append(i)
        continue

      if re.match(CR_DEFINE_START_REGEX, line):
        assert cr_define_start_index == -1, (
            'Multiple cr.define() calls not supported.')
        assert cr_define_end_index == -1, 'Unexpected #cr_define_end'
        cr_define_start_index = i
        indices_to_remove.append(i)
        continue

      if re.match(CR_DEFINE_END_REGEX, line):
        assert cr_define_start_index >= 0, 'Unexpected #cr_define_end'
        assert cr_define_end_index == -1, (
            'Multiple #cr_define_end calls not supported.')
        cr_define_end_index = i
        indices_to_remove.append(i)
        ignore_remaining_lines = True
        continue

      line = line.replace(EXPORT_LINE_REGEX, 'export')
      line = line.replace(IMPORT_LINE_REGEX, 'import')
      line = _rewrite_namespaces(line, namespace_rewrites)
      lines[i] = line


  # Process line numbers in descending order, such that the array can be
  # modified in-place.
  indices_to_remove.reverse()
  for i in indices_to_remove:
    del lines[i]

  out_filename = os.path.splitext(os.path.basename(filename))[0] + '.m.js'

  # Reconstruct file.
  # Specify the newline character so that the exact same file is generated
  # across platforms.
  with io.open(os.path.join(out_folder, out_filename), 'w', newline='\n') as f:
    for l in lines:
      f.write(unicode(l, 'utf-8'))
  return

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--input_files', nargs='*', required=True)
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--namespace_rewrites', required=False, nargs="*")
  args = parser.parse_args(argv)

  # Extract namespace rewrites from arguments.
  namespace_rewrites = {}
  if args.namespace_rewrites:
    for r in args.namespace_rewrites:
      before, after = r.split('|')
      namespace_rewrites[before] = after

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  for f in args.input_files:
    ProcessFile(os.path.join(in_folder, f), out_folder, namespace_rewrites)


if __name__ == '__main__':
  main(sys.argv[1:])

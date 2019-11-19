#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a filename as an argument, sort the #include/#imports in that file.

Shows a diff and prompts for confirmation before doing the deed.
Works great with tools/git/for-all-touched-files.py.
"""

from __future__ import print_function

import optparse
import os
import sys

from yes_no import YesNo


def IsInclude(line):
  """Returns True if the line is an #include/#import/import line."""
  return any([line.startswith('#include '), line.startswith('#import '),
              line.startswith('import ')])


def IncludeCompareKey(line, for_blink):
  """Sorting comparator key used for comparing two #include lines.

  Returns an integer, optionally followed by a string. The integer is used
  for coarse sorting of different categories of headers, and the string is
  used for fine sorting of headers within categeries.
  """
  for prefix in ('#include ', '#import ', 'import '):
    if line.startswith(prefix):
      line = line[len(prefix):]
      break

  if for_blink:
    # Blink likes to have its "config.h" include first.
    if line.startswith('"config.h"'):
      return '0'

    # Blink sorts system headers after others. This is handled by sorting
    # alphabetically so no need to do anything tricky.
    return '1' + line

  # The win32 api has all sorts of implicit include order dependencies :-/
  # Give a few headers special sort keys that make sure they appear before all
  # other headers.
  if line.startswith('<windows.h>'):  # Must be before e.g. shellapi.h
    return '0'
  if line.startswith('<atlbase.h>'):  # Must be before atlapp.h.
    return '1' + line
  if line.startswith('<ole2.h>'):  # Must be before e.g. intshcut.h
    return '1' + line
  if line.startswith('<unknwn.h>'):  # Must be before e.g. intshcut.h
    return '1' + line

  # C++ system headers should come after C system headers.
  if line.startswith('<'):
    if line.find('.h>') != -1:
      return '2' + line.lower()
    else:
      return '3' + line.lower()

  return '4' + line


def SortHeader(infile, outfile, for_blink):
  """Sorts the headers in infile, writing the sorted file to outfile."""
  def CompareKey(line):
    return IncludeCompareKey(line, for_blink)

  for line in infile:
    if IsInclude(line):
      headerblock = []
      while IsInclude(line):
        infile_ended_on_include_line = False
        headerblock.append(line)
        # Ensure we don't die due to trying to read beyond the end of the file.
        try:
          line = infile.next()
        except StopIteration:
          infile_ended_on_include_line = True
          break
      for header in sorted(headerblock, key=CompareKey):
        outfile.write(header)
      if infile_ended_on_include_line:
        # We already wrote the last line above; exit to ensure it isn't written
        # again.
        return
      # Intentionally fall through, to write the line that caused
      # the above while loop to exit.
    outfile.write(line)


def FixFileWithConfirmFunction(filename, confirm_function,
                               perform_safety_checks, for_blink=False):
  """Creates a fixed version of the file, invokes |confirm_function|
  to decide whether to use the new file, and cleans up.

  |confirm_function| takes two parameters, the original filename and
  the fixed-up filename, and returns True to use the fixed-up file,
  false to not use it.

  If |perform_safety_checks| is True, then the function checks whether it is
  unsafe to reorder headers in this file and skips the reorder with a warning
  message in that case.
  """
  if perform_safety_checks and IsUnsafeToReorderHeaders(filename):
    print(
        'Not reordering headers in %s as the script thinks that the '
        'order of headers in this file is semantically significant.' % filename)
    return
  fixfilename = filename + '.new'
  infile = open(filename, 'rb')
  outfile = open(fixfilename, 'wb')
  SortHeader(infile, outfile, for_blink)
  infile.close()
  outfile.close()  # Important so the below diff gets the updated contents.

  try:
    if confirm_function(filename, fixfilename):
      if sys.platform == 'win32':
        os.unlink(filename)
      os.rename(fixfilename, filename)
  finally:
    try:
      os.remove(fixfilename)
    except OSError:
      # If the file isn't there, we don't care.
      pass


def DiffAndConfirm(filename, should_confirm, perform_safety_checks, for_blink):
  """Shows a diff of what the tool would change the file named
  filename to.  Shows a confirmation prompt if should_confirm is true.
  Saves the resulting file if should_confirm is false or the user
  answers Y to the confirmation prompt.
  """
  def ConfirmFunction(filename, fixfilename):
    diff = os.system('diff -u %s %s' % (filename, fixfilename))
    if sys.platform != 'win32':
      diff >>= 8
    if diff == 0:  # Check exit code.
      print('%s: no change' % filename)
      return False

    return (not should_confirm or YesNo('Use new file (y/N)?'))

  FixFileWithConfirmFunction(filename, ConfirmFunction, perform_safety_checks,
                             for_blink)

def IsUnsafeToReorderHeaders(filename):
  # *_message_generator.cc is almost certainly a file that generates IPC
  # definitions. Changes in include order in these files can result in them not
  # building correctly.
  if filename.find("message_generator.cc") != -1:
    return True
  return False

def main():
  parser = optparse.OptionParser(usage='%prog filename1 filename2 ...')
  parser.add_option('-f', '--force', action='store_false', default=True,
                    dest='should_confirm',
                    help='Turn off confirmation prompt.')
  parser.add_option('--no_safety_checks',
                    action='store_false', default=True,
                    dest='perform_safety_checks',
                    help='Do not perform the safety checks via which this '
                    'script refuses to operate on files for which it thinks '
                    'the include ordering is semantically significant.')
  parser.add_option('--for_blink', action='store_true', default=False,
                    dest='for_blink', help='Whether the blink header sorting '
                    'rules should be applied.')
  opts, filenames = parser.parse_args()

  if len(filenames) < 1:
    parser.print_help()
    return 1

  for filename in filenames:
    DiffAndConfirm(filename, opts.should_confirm, opts.perform_safety_checks,
                   opts.for_blink)


if __name__ == '__main__':
  sys.exit(main())

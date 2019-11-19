# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import difflib
import logging
import os
import shutil
import sys

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 os.pardir, os.pardir, 'python', 'google'))
import path_utils

import diff_util

def DoPresubmit(argv, original_filename, backup_filename, script_name,
                prettyFn):
  """Execute presubmit/pretty printing for the target file.

  Args:
    argv: command line arguments
    original_filename: The filename to read from.
    backup_filename: When pretty printing, move the old file contents here.
    script_name: The name of the script to run for pretty printing.
    prettyFn: A function which takes the original xml content and produces
        pretty printed xml.

  Returns:
    An exit status.  Non-zero indicates errors.
  """
  # interactive: Print log info messages and prompt user to accept the diff.
  interactive = ('--non-interactive' not in argv)
  # presubmit: Simply print a message if the input is not formatted correctly.
  presubmit = ('--presubmit' in argv)
  # diff: Print diff to stdout rather than modifying files.
  diff = ('--diff' in argv)

  if interactive:
    logging.basicConfig(level=logging.INFO)
  else:
    logging.basicConfig(level=logging.ERROR)

  # If there is a description xml in the current working directory, use that.
  # Otherwise, use the one residing in the same directory as this script.
  xml_dir = os.getcwd()
  if not os.path.isfile(os.path.join(xml_dir, original_filename)):
    xml_dir = path_utils.ScriptDir()

  xml_path = os.path.join(xml_dir, original_filename)

  # Save the original file content.
  logging.info('Loading %s...', os.path.relpath(xml_path))
  with open(xml_path, 'rb') as f:
    original_xml = f.read()

  # Check there are no CR ('\r') characters in the file.
  if '\r' in original_xml:
    logging.error('DOS-style line endings (CR characters) detected - these are '
                  'not allowed. Please run dos2unix %s', original_filename)
    return 1

  try:
    pretty = prettyFn(original_xml)
  except Exception as e:
    logging.exception('Aborting parsing due to fatal errors:')
    return 1

  if original_xml == pretty:
    logging.info('%s is correctly pretty-printed.', original_filename)
    return 0

  if presubmit:
    logging.error('%s is not formatted correctly; run %s to fix.',
                  original_filename, script_name)
    return 1

  # Prompt user to consent on the change.
  if interactive and not diff_util.PromptUserToAcceptDiff(
      original_xml, pretty, 'Is the new version acceptable?'):
    logging.error('Diff not accepted. Aborting.')
    return 1

  if diff:
    for line in difflib.unified_diff(original_xml.splitlines(),
                                     pretty.splitlines()):
      print(line)
    return 0

  logging.info('Creating backup file: %s', backup_filename)
  shutil.move(xml_path, os.path.join(xml_dir, backup_filename))

  with open(xml_path, 'wb') as f:
    f.write(pretty)
  logging.info('Updated %s. Don\'t forget to add it to your changelist',
               xml_path)
  return 0


def DoPresubmitMain(*args, **kwargs):
  sys.exit(DoPresubmit(*args, **kwargs))

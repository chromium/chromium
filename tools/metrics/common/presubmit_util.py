# Copyright 2024 The Chromium Authors
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


def DoPresubmit(argv,
                original_filename,
                backup_filename,
                prettyFn,
                script_name='git cl format'):
  """Execute presubmit/pretty printing for the target file.

  Args:
    argv: command line arguments
    original_filename: The path to the file to read from.
    backup_filename: When pretty printing, move the old file contents here.
    prettyFn: A function which takes the original xml content and produces
        pretty printed xml.
    script_name: The name of the script to run for pretty printing.

  Returns:
    An exit status.  Non-zero indicates errors.
  """
  # interactive: Print log info messages and prompt user to accept the diff.
  interactive = ('--non-interactive' not in argv)
  # presubmit: Simply print a message if the input is not formatted correctly.
  presubmit = ('--presubmit' in argv)
  # diff: Print diff to stdout rather than modifying files.
  diff = ('--diff' in argv)
  # cleanup: Remove the backup file after at the end, if created.
  cleanup = ('--cleanup' in argv)

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
  if b'\r' in original_xml:
    logging.error('DOS-style line endings (CR characters) detected - these are '
                  'not allowed. Please run dos2unix %s', original_filename)
    return 1

  original_xml = original_xml.decode('utf-8')

  try:
    pretty = prettyFn(original_xml)
  except Exception as e:
    logging.exception('Aborting parsing due to fatal errors:')
    return 1

  if original_xml == pretty:
    logging.info('%s is correctly pretty-printed.', original_filename)
    return 0

  if presubmit:
    if interactive:
      logging.error('%s is not formatted correctly; run `%s` to fix.',
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
  backup_path = os.path.join(xml_dir, backup_filename)
  shutil.move(xml_path, backup_path)

  pretty = pretty.encode('utf-8')
  with open(xml_path, 'wb') as f:
    f.write(pretty)
  logging.info('Updated %s. Don\'t forget to add it to your changelist',
               xml_path)

  # Remove backup file if created, if prompted by user.
  if cleanup and backup_path:
    logging.info('Cleaning up backup file: %s' % backup_filename)
    os.remove(backup_path)

  return 0


def DoPresubmitMain(*args, **kwargs):
  sys.exit(DoPresubmit(*args, **kwargs))


def CheckChange(xml_file, input_api, output_api):
  """Checks that xml is pretty-printed and well-formatted."""
  absolute_paths_of_affected_files = [
      f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
  ]
  xml_file_changed = any([
      input_api.basename(p) == xml_file
      and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()
      for p in absolute_paths_of_affected_files
  ])

  if not xml_file_changed:
    return []

  cwd = input_api.PresubmitLocalPath()
  pretty_print_args = [
      input_api.python3_executable, 'pretty_print.py', '--presubmit', xml_file
  ]

  exit_code = input_api.subprocess.call(pretty_print_args, cwd=cwd)
  if exit_code != 0:
    return [
        output_api.PresubmitError(
            '%s is not prettified; run `git cl format` to fix.' % xml_file),
    ]

  validate_format_args = [
      input_api.python3_executable, 'validate_format.py', '--presubmit',
      xml_file
  ]
  exit_code = input_api.subprocess.call(validate_format_args, cwd=cwd)
  if exit_code != 0:
    return [
        output_api.PresubmitError(
            '%s does not pass format validation; run %s/validate_format.py '
            'and fix the reported error(s) or warning(s).' %
            (xml_file, input_api.PresubmitLocalPath())),
    ]

  return []

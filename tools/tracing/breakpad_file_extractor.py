# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Uses dump_syms to extract brekapad symbol files
"""

import os
import logging
import subprocess


def ExtractBreakpadFiles(dump_syms_path, build_dir, breakpad_output_dir):
  """Uses dump_syms to extract breakpad files.

  Args:
    dump_syms_path: The path to the dump_syms binary that should be run.
    build_dir: The path to the input directory containing the binaries that
      dump_syms will use. If the directory '|build_dir|/lib.unstipped'
      exists, dump_syms is run on this directory instead.
    breakpad_base_dir: The output directory for the breakpad symbol files
      produced.

  Raises:
    FileNotFoundError: If the dump_syms binary or the input and output
      directories cannot be found.
    Exception: If none of the files in |build_dir| could be symbolized.
  """
  # Check to see if |dump_syms_path| is a file.
  dump_syms_binary = _GetDumpSyms(dump_syms_path, build_dir)
  if dump_syms_binary is None:
    raise FileNotFoundError(
        'dump_syms is missing. you can build dump_syms in the {build_dir} by'
        'running ninja -C {build_dir} dump_syms'.format(build_dir=build_dir))

  # Check if |build_dir| and |breakpad_base_dir| are directories.
  if not os.path.isdir(build_dir):
    raise FileNotFoundError('Invalid build directory.')
  if not os.path.isdir(breakpad_output_dir):
    raise FileNotFoundError('Invalid breakpad output directory.')

  # If on Android, lib.unstripped will hold symbols for binaries.
  symbol_dir = build_dir
  if os.path.isdir(os.path.join(build_dir, 'lib.unstripped')):
    symbol_dir = os.path.join(build_dir, 'lib.unstripped')

  breakpad_file_count = 0
  for file in os.listdir(symbol_dir):
    input_file_path = os.path.join(symbol_dir, file)
    if os.path.isfile(input_file_path) and _IsValidBinaryPath(input_file_path):
      # Construct absolute file paths for input and output files.
      output_file_path = os.path.join(breakpad_output_dir, file + '.breakpad')
      if _RunDumpSyms(dump_syms_binary, input_file_path, output_file_path):
        breakpad_file_count += 1

  # Extracting breakpad symbols should be successful with at least one file.
  if breakpad_file_count == 0:
    raise Exception(
        'Could not create breakpad symbols from any files from {symbol_dir}.'.
        format(symbol_dir=symbol_dir))


def _RunDumpSyms(dump_syms_binary, input_file_path, output_file_path):
  """Runs the dump_syms binary on a file and outputs the resulting breakpad
     symbols to the specified file.
  Args:
    cmd: The command to run dump_syms.
    output_file_path: The file path for the output breakpad symbol file.

  Returns:
    True if the command succeeded and false otherwise.
  """
  cmd = [dump_syms_binary, '-c', '-r', input_file_path]
  with open(output_file_path, 'w') as f:
    proc = subprocess.Popen(cmd, stdout=f, stderr=subprocess.PIPE)
  stderr = proc.communicate()[1]
  if proc.returncode != 0:
    logging.warning('{dump_syms_err}'.format(dump_syms_err=str(stderr)))
    logging.debug(
        'Could not create breakpad symbols {file}'.format(file=input_file_path))
    return False
  logging.debug(
      'Created breakpad symbols from {file}'.format(file=input_file_path))
  return True


def _GetDumpSyms(dump_syms_path, build_dir):
  """Checks to see if dump_syms can be found.

  Args:
    dump_syms_path: The given path to dump_syms.
    build_dir: The path to a directory.

  Returns:
    The path to the dump_syms binary or None if none is found.
    """
  if dump_syms_path is not None and os.path.isfile(
      dump_syms_path) and 'dump_syms' in dump_syms_path:
    return dump_syms_path
  path_to_dump_syms = os.path.join(build_dir, 'dump_syms')
  if os.path.isfile(path_to_dump_syms):
    return path_to_dump_syms
  return None


def _IsValidBinaryPath(path):
  # Get the file name from the full file path.
  file_name = os.path.basename(path)
  if file_name.endswith('partition.so') or file_name.endswith(
      '.dwp') or file_name.endswith('.dwo') or '_combined' in file_name:
    return False
  return 'chrome' in file_name or file_name.endswith(
      '.so') or file_name.endswith('.exe')

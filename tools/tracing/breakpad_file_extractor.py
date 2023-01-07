# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uses dump_syms to extract breakpad symbol files."""

import logging
import os
import subprocess
import sys

import flag_utils
import rename_breakpad

sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
                 'catapult', 'common', 'py_utils'))
from py_utils import tempfile_ext


def ExtractBreakpadFiles(dump_syms_path,
                         build_dir,
                         breakpad_output_dir,
                         search_unstripped=True,
                         module_ids=None):
  """Uses dump_syms to extract breakpad files.

  Args:
    dump_syms_path: The path to the dump_syms binary that should be run.
    build_dir: The path to the input directory containing the binaries that
      dump_syms will use.
    breakpad_output_dir: The output directory for the breakpad symbol files
      produced.
    search_unstripped: Boolean flag for whether to search for 'lib.unstripped'
      subdirectory or not. If specified and '|build_dir|/lib.unstripped' exists,
      dump_syms is run on this directory instead. If not specified, dump_syms is
      run on |build_dir|.
    module_ids: A set of module IDs needed to symbolize the trace. Only extracts
      breakpad on symbol binaries with a module ID in this set. Extracts all
      symbols if |module_ids| is None.

  Returns:
    True if at least one breakpad file could be extracted from |build_dir|;
    False, otherwise

  Raises:
    Exception: If the dump_syms binary or the input and output
      directories cannot be found.
  """
  # Check to see if |dump_syms_path| is a file.
  dump_syms_path = EnsureDumpSymsBinary(dump_syms_path, build_dir)

  # Check if |build_dir| and |breakpad_base_dir| are directories.
  if not os.path.isdir(build_dir):
    raise Exception('Invalid build directory.')
  if not os.path.isdir(breakpad_output_dir):
    raise Exception('Invalid breakpad output directory.')

  # If on Android, lib.unstripped will hold symbols for binaries.
  symbol_dir = build_dir
  if search_unstripped and os.path.isdir(
      os.path.join(build_dir, 'lib.unstripped')):
    symbol_dir = os.path.join(build_dir, 'lib.unstripped')

  breakpad_file_count = 0
  for file_iter in os.listdir(symbol_dir):
    input_file_path = os.path.join(symbol_dir, file_iter)
    if os.path.isfile(input_file_path) and IsValidBinaryPath(input_file_path):
      if not IsModuleNeededForSymbolization(dump_syms_path, module_ids,
                                            input_file_path):
        continue
      # Construct absolute file paths for input and output files.
      output_file_path = os.path.join(breakpad_output_dir,
                                      file_iter + '.breakpad')

      flag_utils.GetTracingLogger().debug('Extracting breakpad file from: %s',
                                          input_file_path)
      if _RunDumpSyms(dump_syms_path, input_file_path, output_file_path):
        flag_utils.GetTracingLogger().debug('Extracted breakpad to: %s',
                                            output_file_path)
        breakpad_file_count += 1

  # Extracting breakpad symbols should be successful with at least one file.
  return breakpad_file_count > 0


def ExtractBreakpadOnSubtree(symbols_root, metadata, dump_syms_path):
  """Converts symbol files in the given subtree into breakpad files.

  Args:
    symbols_root: root of subtree containing symbol files to convert to breakpad
      format.
    metadata: trace metadata to extract module ids from.
    dump_syms_path: local path to dump_syms binary.

  Raises:
    Exception: if path to dump_syms binary not passed or no breakpad files
      could be extracted from subtree.
  """
  flag_utils.GetTracingLogger().debug('Converting symbols to breakpad format.')
  if dump_syms_path is None:
    raise Exception('Path to dump_syms binary is required for symbolizing '
                    'official Android traces. You can build dump_syms from '
                    'your local build directory with the right architecture '
                    'with: autoninja -C out_<arch>/Release dump_syms.')

  # Set of module IDs we need to symbolize.
  module_ids = GetModuleIdsToSymbolize(metadata)

  did_extract = False
  for root_dir, _, _ in os.walk(symbols_root, topdown=True):
    root_path = os.path.abspath(root_dir)
    did_extract |= ExtractBreakpadFiles(dump_syms_path,
                                        root_path,
                                        root_path,
                                        search_unstripped=False,
                                        module_ids=module_ids)

  if not did_extract:
    raise Exception(
        'No breakpad symbols could be extracted from files in the subtree: ' +
        symbols_root)


def GetModuleIdsToSymbolize(metadata):
  """Returns module IDs needed for symbolization and logs breakpad message.

  We log the message before calling |ExtractBreakpadFiles| because otherwise
  we will repeatedly log when |ExtractBreakpadOnSubtree| recursively runs
  |ExtractBreakpadFiles|.

  Args:
    metadata: metadata extracted from the trace.
  """
  module_ids = metadata.GetModuleIds()

  if module_ids is None:
    flag_utils.GetTracingLogger().info(
        'No specified modules to extract. Converting all symbol '
        'binaries to breakpad.')
  else:
    flag_utils.GetTracingLogger().debug('Module IDs to symbolize: %s',
                                        (module_ids))

  return module_ids


def IsModuleNeededForSymbolization(dump_syms_path, module_ids, symbol_binary):
  """Determines if we should extract breakpad from symbol binary.

  If module_ids is None, then we extract breakpad on all symbol binaries.
  Otherwise, we only extract breakpad on binaries with a module ID needed to
  symbolize the trace.

  Args:
    dump_syms_path: The path to the dump_syms binary that should be run.
    module_ids: A set of module IDs needed to symbolize the trace. Only extracts
      breakpad on symbol binaries with a module ID in this set. Extracts all
      symbols if |module_ids| is None.
    symbol_binary: Symbol binary file to symbolize trace.

  Returns:
    True if symbols should be extracted to breakpad; false, otherwise.
  """
  if module_ids is None:
    return True

  # Only convert breakpad if binary has module ID we need to symbolize.
  module_id = _GetModuleIDFromBinary(dump_syms_path, symbol_binary)
  if module_id is None or module_id not in module_ids:
    flag_utils.GetTracingLogger().debug(
        'Skipping breakpad extraction for module (%s, %s) '
        'since trace has no frames with this ID.', module_id, symbol_binary)
    return False
  return True


def _GetModuleIDFromBinary(dump_syms_path, symbol_binary):
  """Gets module ID of symbol binary.

  Args:
    dump_syms_path: The path to the dump_syms binary that should be run.
    symbol_binary: path to symbol binary.

  Returns:
    Module ID from symbol binary, or None if fails to extract.
  """
  # Creates temp file because |_RunDumpSyms| pipes result into a file.
  # After extracting the module ID, we do not need this output file.
  with tempfile_ext.NamedTemporaryFile(mode='w+') as output_file:
    output_file.close()  # RunDumpsyms opens the file again.
    if not _RunDumpSyms(dump_syms_path,
                        symbol_binary,
                        output_file.name,
                        only_module_header=True):
      return None
    return rename_breakpad.ExtractModuleIdIfValidBreakpad(output_file.name)


def _RunDumpSyms(dump_syms_binary,
                 input_file_path,
                 output_file_path,
                 only_module_header=False):
  """Runs the dump_syms binary on a file and outputs the resulting breakpad.

     symbols to the specified file.
  Args:
    dump_syms_binary: The path to the dump_syms binary that should be run.
    input_file_path: Input file path to run dump_syms on.
    output_file_path: Output file path to store result.
    only_module_header: Only extracts the module header, if specified.

  Returns:
    True if the command succeeded and false otherwise.
  """
  cmd = [dump_syms_binary]
  if only_module_header:
    cmd.append('-i')
  cmd.append(input_file_path)

  with open(output_file_path, 'w') as f:
    proc = subprocess.Popen(cmd, stdout=f, stderr=subprocess.PIPE)
  stderr = proc.communicate()[1]
  if proc.returncode != 0:
    flag_utils.GetTracingLogger().info(
        'Dump_syms failed to extract information from symbol binary: %s. '
        'Error: %s', input_file_path, str(stderr))
    return False
  return True


def EnsureDumpSymsBinary(dump_syms_path, build_dir):
  """Checks to see if dump_syms can be found.

  Args:
    dump_syms_path: The given path to dump_syms.
    build_dir: The path to a directory.

  Returns:
    The path to the dump_syms binary or raises exception.
  """
  if dump_syms_path is not None and os.path.isfile(
      dump_syms_path) and 'dump_syms' in dump_syms_path:
    return dump_syms_path

  if build_dir is not None:
    path_to_dump_syms = os.path.join(build_dir, 'dump_syms')
    if os.path.isfile(path_to_dump_syms):
      return path_to_dump_syms

  if not build_dir:
    build_dir = 'out/android'  # For error message.
  raise Exception(
      'dump_syms binary not found. Build a binary with '
      'autoninja -C {build_dir} dump_syms and try again with '
      '--dump_syms={build_dir}/dump_syms'.format(build_dir=build_dir))


def IsValidBinaryPath(path):
  # Get the file name from the full file path.
  file_name = os.path.basename(path)
  if file_name.endswith('partition.so') or file_name.endswith(
      '.dwp') or file_name.endswith('.dwo') or '_combined' in file_name:
    return False
  return file_name == 'chrome' or file_name.endswith(
      '.so') or file_name.endswith('.exe')

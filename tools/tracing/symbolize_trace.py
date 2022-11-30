# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Symbolizes a perfetto trace file."""

import logging
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
                 'catapult', 'systrace'))

import breakpad_file_extractor
import flag_utils
import metadata_extractor
import rename_breakpad
import symbol_fetcher


def SymbolizeTrace(trace_file, options):
  """Symbolizes a perfetto trace file.

  Args:
    trace_file: path to proto trace file to symbolize.
    options: The options set by the command line args.

  Raises:
    Exception: if breakpad_output_dir is not empty or if path to local breakpad
      directory is invalid.
  """
  print('Symbolizing file')

  need_cleanup = False
  if options.local_breakpad_dir is None:
    breakpad_file_extractor.EnsureDumpSymsBinary(options.dump_syms_path,
                                                 options.local_build_dir)

    # Ensure valid |breakpad_output_dir|
    if options.breakpad_output_dir is None:
      # Create a temp dir if output dir is not provided.
      # Temp dir must be cleaned up later.
      options.breakpad_output_dir = tempfile.mkdtemp()
      need_cleanup = True
      flag_utils.GetTracingLogger().warning(
          'Created temporary directory to hold symbol files. These symbols '
          'will be cleaned up after symbolization. Please specify '
          '--breakpad_output_dir=<cached_dir> to save the symbols, if you need '
          'to profile multiple times. The future runs need to use '
          '--local_breakpad_dir=<cached_dir> flag so the symbolizer uses the '
          'cache.')
    else:
      if os.path.isdir(options.breakpad_output_dir):
        if os.listdir(options.breakpad_output_dir):
          raise Exception('Breakpad output directory is not empty: ' +
                          options.breakpad_output_dir)
      else:
        os.makedirs(options.breakpad_output_dir)
        flag_utils.GetTracingLogger().debug(
            'Created directory to hold symbol files.')
  else:
    if not os.path.isdir(options.local_breakpad_dir):
      raise Exception('Local breakpad directory is not valid.')
    options.breakpad_output_dir = options.local_breakpad_dir

  _EnsureBreakpadSymbols(trace_file, options)

  # Set output file to write trace data and symbols to.
  if options.output_file is None:
    options.output_file = os.path.join(
        os.path.dirname(trace_file),
        os.path.basename(trace_file) + '_symbolized_trace')

  _Symbolize(trace_file, options.symbolizer_path, options.breakpad_output_dir,
             options.output_file)

  print('Symbolized trace saved to: ' + os.path.abspath(options.output_file))

  # Cleanup
  if need_cleanup:
    flag_utils.GetTracingLogger().debug('Cleaning up symbol files.')
    shutil.rmtree(options.breakpad_output_dir)


def _EnsureBreakpadSymbols(trace_file, options):
  """Ensures that there are breakpad symbols to symbolize with.

  Args:
    trace_file: The trace file to be symbolized.
    options: The options set by the command line args. This is used to check if
      symbols need to be fetched, extracted, or if they are already present.

  Raises:
    Exception: if no breakpad files could be extracted.
  """
  # If |options.local_breakpad_dir| is not None, then this can be skipped and
  # |trace_file| can be symbolized using those symbols.
  if options.local_breakpad_dir is not None:
    return

  # Extract Metadata
  flag_utils.GetTracingLogger().info('Extracting proto trace metadata.')
  trace_metadata = metadata_extractor.MetadataExtractor(
      options.trace_processor_path, trace_file)
  trace_metadata.Initialize()
  flag_utils.GetTracingLogger().info(trace_metadata)

  if options.local_build_dir is not None:
    # Extract breakpad symbol files from binaries in |options.local_build_dir|.
    if not breakpad_file_extractor.ExtractBreakpadFiles(
        options.dump_syms_path,
        options.local_build_dir,
        options.breakpad_output_dir,
        search_unstripped=True,
        module_ids=breakpad_file_extractor.GetModuleIdsToSymbolize(
            trace_metadata)):
      raise Exception(
          'No breakpad symbols could be extracted from files in: %s xor %s' %
          (options.local_build_dir,
           os.path.join(options.local_build_dir, 'lib.unstripped')))

    rename_breakpad.RenameBreakpadFiles(options.breakpad_output_dir,
                                        options.breakpad_output_dir)
    return

  # Fetch trace breakpad symbols from GCS
  flag_utils.GetTracingLogger().info(
      'Fetching and extracting trace breakpad symbols.')
  symbol_fetcher.GetTraceBreakpadSymbols(options.cloud_storage_bucket,
                                         trace_metadata,
                                         options.breakpad_output_dir,
                                         options.dump_syms_path)


def _Symbolize(trace_file, symbolizer_path, breakpad_output_dir, output_file):
  """"Symbolizes a trace.

  Args:
    trace_file: The trace file to be symbolized.
    symbolizer_path: The path to the trace_to_text tool to use.
    breakpad_output_dir: Contains the breakpad symbols to use for symbolization.
    output_file: The path to the file to output symbols to.

  Raises:
    RuntimeError: If _RunSymbolizer() fails to execute the given command.
  """
  # Set environment variable as location of stored breakpad files.
  symbolize_env = os.environ.copy()
  symbolize_env['BREAKPAD_SYMBOL_DIR'] = os.path.join(breakpad_output_dir, '')
  cmd = [symbolizer_path, 'symbolize', trace_file]

  # Open temporary file where symbols can be stored.
  with tempfile.TemporaryFile(mode='wb+') as temp_symbol_file:
    _RunSymbolizer(cmd, symbolize_env, temp_symbol_file)

    # Write trace data and symbol data to the same file.
    temp_symbol_file.seek(0)
    symbol_data = temp_symbol_file.read()
    with open(trace_file, 'rb') as f:
      trace_data = f.read()
    with open(output_file, 'wb') as f:
      f.write(trace_data)
      f.write(symbol_data)
      flag_utils.GetTracingLogger().info(
          'Symbolized %s(%d bytes) with %d bytes of symbol data',
          os.path.abspath(trace_file), len(trace_data), len(symbol_data))


def _RunSymbolizer(cmd, env, stdout):
  proc = subprocess.Popen(cmd, env=env, stdout=stdout, stderr=subprocess.PIPE)
  out, stderr = proc.communicate()
  flag_utils.GetTracingLogger().debug('STDOUT:%s', str(out))
  flag_utils.GetTracingLogger().debug('STDERR:%s', str(stderr))
  if proc.returncode != 0:
    raise RuntimeError(str(stderr))

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Symbolizes a perfetto trace file.
"""

import os
import sys
import shutil
import tempfile
import logging
import subprocess

sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
                 'catapult', 'systrace'))

from systrace import util
import metadata_extractor
import symbol_fetcher


def SymbolizeTrace(trace_file, options):
  """Symbolizes a perfetto trace file.

  Args:
    trace_file: path to proto trace file to symbolize.
    cloud_storage_bucket: bucket in cloud storage where symbols reside.
    trace_processor_path: path to the trace_processor executable. If not
      specified, trace processor binary will be automatically downloaded.
    breakpad_output_dir: empty local base directory of where to save
      breakpad symbols.

  Raises:
    Exception: if breakpad_output_dir is not empty.
  """
  need_cleanup = False
  if options.local_breakpad_dir is None:
    # Ensure valid |breakpad_output_dir|
    if options.breakpad_output_dir is None:
      # Create a temp dir if output dir is not provided.
      # Temp dir must be cleaned up later.
      options.breakpad_output_dir = tempfile.mkdtemp()
      need_cleanup = True
      logging.debug('Created temporary directory to hold symbol files.')
    else:
      if not os.path.isdir(options.breakpad_output_dir):
        os.makedirs(options.breakpad_output_dir)
        logging.debug('Created directory to hold symbol files.')
      else:
        # Assert breakpad_output_dir is empty
        if os.listdir(options.breakpad_output_dir):
          raise Exception('Breakpad output directory is not empty: ' +
                          options.breakpad_output_dir)
  else:
    if not os.path.isdir(options.local_breakpad_dir):
      raise FileNotFoundError('Local breakpad directory is not valid.')
    options.breakpad_output_dir = options.local_breakpad_dir

  if options.local_build_dir is None and options.local_breakpad_dir is None:
    # Extract Metadata
    logging.info('Extracting proto trace metadata.')
    trace_metadata = metadata_extractor.MetadataExtractor(
        options.trace_processor_path, trace_file)
    trace_metadata.Initialize()
    logging.info(trace_metadata)

    # Fetch trace breakpad symbols from GCS
    logging.info('Fetching and extracting trace breakpad symbols.')
    symbol_fetcher.GetTraceBreakpadSymbols(options.cloud_storage_bucket,
                                           trace_metadata,
                                           options.breakpad_output_dir)

  # TODO(uwemwilson): Add call to function to extract breakpad symbol files from
  # local build directory.

  # Set output file to write trace data and symbols to.
  if options.output_file is None:
    options.output_file = os.path.join(
        os.path.dirname(trace_file),
        os.path.basename(trace_file) + '_symbolized_trace')

  _Symbolize(trace_file, options.symbolizer_path, options.breakpad_output_dir,
             options.output_file)

  # Cleanup
  if need_cleanup:
    logging.debug('Cleaning up symbol files.')
    shutil.rmtree(options.breakpad_output_dir)


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
  symbolize_env['BREAKPAD_SYMBOL_DIR'] = os.path.join(breakpad_output_dir, "")
  cmd = [symbolizer_path, 'symbolize', trace_file]

  # Open temporary file where symbols can be stored.
  with tempfile.TemporaryFile(mode='w+') as temp_symbol_file:
    _RunSymbolizer(cmd, symbolize_env, temp_symbol_file)

    # Write trace data and symbol data to the same file.
    temp_symbol_file.seek(0)
    symbol_data = temp_symbol_file.read()
    with open(trace_file, 'r') as f:
      trace_data = f.read()
    with open(output_file, 'w') as f:
      f.write(trace_data)
      f.write(symbol_data)


def _RunSymbolizer(cmd, env, stdout):
  proc = subprocess.Popen(cmd, env=env, stdout=stdout, stderr=subprocess.PIPE)
  stderr = proc.communicate()[1]
  if proc.returncode != 0:
    raise RuntimeError(str(stderr))

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper functions for Perfetto profiling (i.e. callstack sampling) in Telemetry
# benchmarks.

import logging
import os
import shutil
import subprocess
import threading

# traceconv is used for symbolization and generating pprof profiles.
_TRACECONV_PATH = os.path.normpath(
    os.path.join(os.path.abspath(__file__),
                 '../../../../../third_party/perfetto/tools/traceconv'))
# traceconv is not guaranteed to be thread-safe.
_TRACECONV_LOCK = threading.Lock()


def _ConcatenateFiles(files_to_concatenate, output_path):
  """Concatenates files in the order provided.

  Args:
    files_to_concatenate: Paths for input files to concatenate.
    output_path: Path to the resultant output file.
  """
  with open(output_path, 'wb') as output_file:
    for input_path in files_to_concatenate:
      with open(input_path, 'rb') as input_file:
        shutil.copyfileobj(input_file, output_file)


def _CopyFiles(source_directory_path, destination_directory_path):
  """Copies all files in from the source to the destination directory."""
  file_names = os.listdir(source_directory_path)
  if file_names is None:
    return
  for file_name in file_names:
    shutil.copy(os.path.join(source_directory_path, file_name),
                destination_directory_path)


def SymbolizeTrace(trace_path):
  """Attempts symbolization of a Perfetto proto trace, if symbols are available.

  If symbolization is successful, the original trace file is replace with the
  symbolized one.

  Args:
    trace_path: The path to the trace file.
  """
  binary_path = os.getenv('PERFETTO_BINARY_PATH')
  if binary_path is None:
    logging.warning(
        'Not symbolizing trace at %s since PERFETTO_BINARY_PATH is not set.',
        trace_path)
    return

  symbols = None
  # Symbolize the trace.
  with _TRACECONV_LOCK:
    popen = subprocess.Popen([_TRACECONV_PATH, 'symbolize', trace_path],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
    stdout, stderr = popen.communicate()

    if popen.return_code == 0:
      symbols = stdout
    else:
      logging.error('Failed to symbolize trace at %s: %s', trace_path,
                    stderr.decode('utf-8'))
  if symbols is None:
    return

  parent_dir = os.path.dirname(trace_path)
  symbols_path = os.path.join(parent_dir, 'symbols')
  symbolized_trace_path = os.path.join(
      parent_dir, 'symbolized_{}'.format(os.path.basename(trace_path)))

  # We write the symbols to a file in case they are useful for debugging, etc.
  with open(symbols_path, 'wb') as symbols_file:
    symbols_file.write(symbols)

  # Add symbols to the trace file.
  _ConcatenateFiles([trace_path, symbols_path], symbolized_trace_path)
  # Replace the original trace file.
  os.remove(trace_path)
  shutil.move(symbolized_trace_path, trace_path)
  logging.info('Successfully symbolized trace at %s', trace_path)


def GenerateProfiles(trace_path):
  """Generates pprof profiles from a Perfetto proto trace.

  Args:
    trace_path: The path to the potentially symbolized Perfetto trace file.
  """
  traceconv_output = None
  with _TRACECONV_LOCK:
    popen = subprocess.Popen([_TRACECONV_PATH, 'profile', '--perf', trace_path],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
    stdout, stderr = popen.communicate()
    if popen.return_code == 0:
      traceconv_output = stdout.decode('utf-8')
    else:
      logging.error('Unable to extract pprof profiles from trace at %s: %s',
                    trace_path, stderr.decode('utf-8'))
  if traceconv_output is None:
    return

  # Copy profiles to the same directory as the source trace.
  profiles_output_directory = None
  for word in traceconv_output.split():
    if 'perf_profile-' in word:
      profiles_output_directory = word
  if profiles_output_directory is None:
    logging.error('No profiles were extracted from trace at %s.', trace_path)
  else:
    _CopyFiles(profiles_output_directory, os.path.dirname(trace_path))
    logging.info('Successfully generated pprof profiles from trace at %s',
                 trace_path)

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

sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
                 'catapult', 'systrace'))

from systrace import util
import metadata_extractor
import symbol_fetcher


def SymbolizeTrace(trace_file,
                   cloud_storage_bucket='chrome-unsigned',
                   trace_processor_path=None,
                   breakpad_output_dir=None):
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
  # Ensure valid |breakpad_output_dir|
  need_cleanup = False
  if breakpad_output_dir is None:
    # Create a temp dir if output dir is not provided.
    # Temp dir must be cleaned up later.
    breakpad_output_dir = tempfile.mkdtemp()
    need_cleanup = True
    logging.debug('Created temporary directory to hold symbol files.')
  else:
    if not os.path.isdir(breakpad_output_dir):
      os.makedirs(breakpad_output_dir)
      logging.debug('Created directory to hold symbol files.')
    else:
      # Assert breakpad_output_dir is empty
      if os.listdir(breakpad_output_dir):
        raise Exception('Breakpad output directory is not empty: ' +
                        breakpad_output_dir)

  # Extract Metadata
  logging.info('Extracting proto trace metadata.')
  trace_metadata = metadata_extractor.MetadataExtractor(trace_processor_path,
                                                        trace_file)
  trace_metadata.Initialize()
  logging.info(trace_metadata)

  # Fetch trace breakpad symbols from GCS
  logging.info('Fetching and extracting trace breakpad symbols.')
  symbol_fetcher.GetTraceBreakpadSymbols(cloud_storage_bucket, trace_metadata,
                                         breakpad_output_dir)

  # TODO(rhuckleberry): use Perfetto's traceconv to symbolize.

  # Cleanup
  if need_cleanup:
    logging.debug('Cleaning up symbol files.')
    shutil.rmtree(breakpad_output_dir)

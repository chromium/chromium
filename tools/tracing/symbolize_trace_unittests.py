#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
import unittest

import symbolize_trace
import symbol_fetcher
import metadata_extractor
import breakpad_file_extractor
import tempfile
import shutil

from unittest.mock import MagicMock


class TestOptions():
  def __init__(self):
    self.trace_processor_path = None
    self.dump_syms_path = None
    self.local_build_dir = None
    self.breakpad_output_dir = None
    self.local_breakpad_dir = None
    self.breakpad_output_dir = None
    self.cloud_storage_bucket = None
    self.output_file = None
    self.symbolizer_path = None


class SymbolizeTraceTestCase(unittest.TestCase):
  def side_effect(self, cmd, env, stdout):
    stdout.write('Symbol data.')

  def setUp(self):
    self.options = TestOptions()
    symbolize_trace._RunSymbolizer = MagicMock(side_effect=self.side_effect)
    symbol_fetcher.GetTraceBreakpadSymbols = MagicMock()
    metadata_extractor.MetadataExtractor = MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles = MagicMock()

  def testNoLocalOrOutputBreakpadDir(self):
    # Test the case with no breakpad output directory specified.
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    # Check logs to see if a breakpad output directory was created.
    with self.assertLogs(level=logging.DEBUG) as logger:
      symbolize_trace.SymbolizeTrace(trace_file, self.options)
    self.assertIn(
        'DEBUG:root:Created temporary directory to hold symbol files.',
        logger.output)
    self.assertIn('DEBUG:root:Cleaning up symbol files.', logger.output)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files.
    os.remove(trace_file)
    os.remove(self.options.output_file)

  def testNoLocalBreakpadDirAndInvalidOutputDir(self):
    self.options.breakpad_output_dir = 'fake/directory'
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    # Check logs to see if a breakpad output directory was created.
    with self.assertLogs(level=logging.DEBUG) as logger:
      symbolize_trace.SymbolizeTrace(trace_file, self.options)
    self.assertIn('DEBUG:root:Created directory to hold symbol files.',
                  logger.output)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testNoLocalBreakpadDirAndValidOutputDir(self):
    self.options.breakpad_output_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    symbolize_trace.SymbolizeTrace(trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testNoLocalBreakpadDirAndNonEmptyBreakpadOutputDir(self):
    self.options.breakpad_output_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    # Check that exception is thrown for non-empty breakpad output directory.
    with tempfile.NamedTemporaryFile(dir=self.options.breakpad_output_dir):
      with self.assertRaisesRegex(Exception,
                                  '^Breakpad output directory is not empty:'):
        symbolize_trace.SymbolizeTrace(trace_file, self.options)

    # Remove files and temp directory.
    os.remove(trace_file)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testInvalidLocalBreakpadDir(self):
    self.options.local_breakpad_dir = 'fake/directory'
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    with self.assertRaisesRegex(FileNotFoundError,
                                'Local breakpad directory is not valid.'):
      symbolize_trace.SymbolizeTrace(trace_file, self.options)

  def testValidLocalBreakpadDir(self):
    self.options.local_breakpad_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    symbolize_trace.SymbolizeTrace(trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_breakpad_dir)

  def testValidLocalBuildDir(self):
    self.options.local_build_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    symbolize_trace.SymbolizeTrace(trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_called_once()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_build_dir)

  def testValidLocalBuildAndBreakpadDir(self):
    self.options.local_build_dir = tempfile.mkdtemp()
    self.options.local_breakpad_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name

    symbolize_trace.SymbolizeTrace(trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
        self.options.output_file,
        os.path.join(os.path.dirname(trace_file),
                     os.path.basename(trace_file) + '_symbolized_trace'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_build_dir)
    shutil.rmtree(self.options.local_breakpad_dir)

  def testOutputFileGiven(self):
    self.options.local_breakpad_dir = tempfile.mkdtemp()
    with tempfile.NamedTemporaryFile(mode='w+',
                                     delete=False) as test_trace_file:
      test_trace_file.write('Trace data.')
      trace_file = test_trace_file.name
      self.options.output_file = os.path.join(os.path.dirname(trace_file),
                                              'output_file')

    symbolize_trace.SymbolizeTrace(trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(self.options.output_file,
                     os.path.join(os.path.dirname(trace_file), 'output_file'))
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(trace_file)
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_breakpad_dir)


if __name__ == '__main__':
  unittest.main()

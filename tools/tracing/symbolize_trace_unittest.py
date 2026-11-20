#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from unittest import mock

import symbolize_trace
import symbol_fetcher
import metadata_extractor
import breakpad_file_extractor
import tempfile
import shutil

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util

path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()


class TestOptions:
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
    if cmd and env:
      stdout.write(b'Symbol data.')

  def setUp(self):
    self.options = TestOptions()

    # Function stashing so mocking doesn't mutate other tests.
    self.RunSymbolizer = symbolize_trace._RunSymbolizer
    self.GetTraceBreakpadSymbols = symbol_fetcher.GetTraceBreakpadSymbols
    self.MetadataExtractor = metadata_extractor.MetadataExtractor
    self.ExtractBreakpadFiles = breakpad_file_extractor.ExtractBreakpadFiles

    symbolize_trace._RunSymbolizer = mock.MagicMock(
      side_effect=self.side_effect
    )
    symbol_fetcher.GetTraceBreakpadSymbols = mock.MagicMock()
    metadata_extractor.MetadataExtractor = mock.MagicMock()
    breakpad_file_extractor.ExtractBreakpadFiles = mock.MagicMock()

    dump_syms_dir = tempfile.mkdtemp()
    self.options.dump_syms_path = os.path.join(dump_syms_dir, 'dump_syms')
    with open(self.options.dump_syms_path, 'w') as _:
      pass

    with tempfile.NamedTemporaryFile(
      mode='w+', delete=False
    ) as test_trace_file:
      test_trace_file.write('Trace data.')
      self.trace_file = test_trace_file.name

  def tearDown(self):
    os.remove(self.trace_file)

    # Unstash functions.
    symbolize_trace._RunSymbolizer = self.RunSymbolizer
    symbol_fetcher.GetTraceBreakpadSymbols = self.GetTraceBreakpadSymbols
    metadata_extractor.MetadataExtractor = self.MetadataExtractor
    breakpad_file_extractor.ExtractBreakpadFiles = self.ExtractBreakpadFiles

  def testNoLocalOrOutputBreakpadDir(self):
    # Test the case with no breakpad output directory specified.
    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files.
    os.remove(self.options.output_file)

  def testNoLocalBreakpadDirAndInvalidOutputDir(self):
    self.options.breakpad_output_dir = 'fake/directory'

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testNoLocalBreakpadDirAndValidOutputDir(self):
    self.options.breakpad_output_dir = tempfile.mkdtemp()

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_called_once()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testNoLocalBreakpadDirAndNonEmptyBreakpadOutputDir(self):
    self.options.breakpad_output_dir = tempfile.mkdtemp()

    # Check that exception is thrown for non-empty breakpad output directory.
    exception_msg = 'Breakpad output directory is not empty:'
    with tempfile.NamedTemporaryFile(dir=self.options.breakpad_output_dir):
      with self.assertRaises(Exception) as e:
        symbolize_trace.SymbolizeTrace(self.trace_file, self.options)
    self.assertIn(exception_msg, str(e.exception))

    # Remove files and temp directory.
    shutil.rmtree(self.options.breakpad_output_dir)

  def testInvalidLocalBreakpadDir(self):
    self.options.local_breakpad_dir = 'fake/directory'

    exception_msg = 'Local breakpad directory is not valid.'
    with self.assertRaises(Exception) as e:
      symbolize_trace.SymbolizeTrace(self.trace_file, self.options)
    self.assertIn(exception_msg, str(e.exception))

  def testFailWhenNoDumpSyms(self):
    self.options.dump_syms_path = None

    exception_msg = 'dump_syms binary not found.'
    with self.assertRaises(Exception) as e:
      symbolize_trace.SymbolizeTrace(self.trace_file, self.options)
    self.assertIn(exception_msg, str(e.exception))

  def testFindDumpSymsInBuild(self):
    self.options.local_build_dir = tempfile.mkdtemp()
    self.options.dump_syms_path = None
    dump_syms_path = os.path.join(self.options.local_build_dir, 'dump_syms')
    with open(dump_syms_path, 'w') as _:
      pass

    # Throws no exception
    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

  def testValidLocalBreakpadDir(self):
    self.options.local_breakpad_dir = tempfile.mkdtemp()

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_breakpad_dir)

  def testValidLocalBuildDir(self):
    self.options.local_build_dir = tempfile.mkdtemp()

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    metadata_extractor.MetadataExtractor.assert_called_once()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_called_once()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_build_dir)

  def testValidLocalBuildAndBreakpadDir(self):
    self.options.local_build_dir = tempfile.mkdtemp()
    self.options.local_breakpad_dir = tempfile.mkdtemp()

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(
        os.path.dirname(self.trace_file),
        os.path.basename(self.trace_file) + '_symbolized_trace',
      ),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_build_dir)
    shutil.rmtree(self.options.local_breakpad_dir)

  def testOutputFileGiven(self):
    self.options.local_breakpad_dir = tempfile.mkdtemp()
    self.options.output_file = os.path.join(
      os.path.dirname(self.trace_file), 'output_file'
    )

    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    metadata_extractor.MetadataExtractor.assert_not_called()
    symbol_fetcher.GetTraceBreakpadSymbols.assert_not_called()
    breakpad_file_extractor.ExtractBreakpadFiles.assert_not_called()
    symbolize_trace._RunSymbolizer.assert_called_once()

    # Check that symbolized trace file was written correctly.
    self.assertEqual(
      self.options.output_file,
      os.path.join(os.path.dirname(self.trace_file), 'output_file'),
    )
    with open(self.options.output_file, 'r') as f:
      symbolized_trace_data = f.read()
      self.assertEqual(symbolized_trace_data, 'Trace data.Symbol data.')

    # Remove files and temp directory.
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_breakpad_dir)

  def testLocalNoBreakpadExtracted(self):
    # Unmock breakpad extraction function.
    breakpad_file_extractor.ExtractBreakpadFiles = self.ExtractBreakpadFiles

    # Set up option arguments to run extract breakpad on local build directory.
    self.options.breakpad_output_dir = tempfile.mkdtemp()
    self.options.local_build_dir = tempfile.mkdtemp()
    trace_file_override = None

    dump_syms_dir = tempfile.mkdtemp()
    self.options.dump_syms_path = os.path.join(dump_syms_dir, 'dump_syms')
    with open(self.options.dump_syms_path, 'w') as _:
      pass

    unstripped_dir = os.path.join(
      self.options.local_build_dir, 'lib.unstripped'
    )
    exception_msg = (
      'No breakpad symbols could be extracted from files in: %s xor %s'
      % (self.options.local_build_dir, unstripped_dir)
    )

    # Test when there is no 'lib.unstripped' subdirectory.
    with self.assertRaises(Exception) as e:
      symbolize_trace.SymbolizeTrace(trace_file_override, self.options)
    self.assertIn(exception_msg, str(e.exception))

    # Test when there is a 'lib.unstripped' subdirectory.
    os.mkdir(unstripped_dir)
    with self.assertRaises(Exception):
      symbolize_trace.SymbolizeTrace(trace_file_override, self.options)

    # Remove files and temp directory.
    shutil.rmtree(self.options.local_build_dir)
    shutil.rmtree(dump_syms_dir)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testLocalBuildDirWithSecondaryAbi(self):
    # A trace can contain 32-bit and 64-bit copies of the same library. Both
    # must be extracted, from the primary and the secondary-ABI build dirs.
    breakpad_file_extractor.ExtractBreakpadFiles = self.ExtractBreakpadFiles
    self.options.breakpad_output_dir = tempfile.mkdtemp()
    self.options.local_build_dir = tempfile.mkdtemp()
    lib_name = 'libwebviewchromium.so'
    module_ids = {
      os.path.join(
        self.options.local_build_dir, 'lib.unstripped', lib_name
      ): '0A07AB57',
      os.path.join(
        self.options.local_build_dir,
        'android_clang_arm',
        'lib.unstripped',
        lib_name,
      ): '892E74C4',
    }
    for path in module_ids:
      os.makedirs(os.path.dirname(path))
      open(path, 'w').close()

    def fake_dump_syms(_, input_file_path, output_file_path, **__):
      with open(output_file_path, 'w') as f:
        f.write(
          'MODULE Linux arm %s %s\n' % (module_ids[input_file_path], lib_name)
        )
      return True

    trace_metadata = metadata_extractor.MetadataExtractor.return_value
    trace_metadata.GetModuleIds.return_value = set(module_ids.values())
    with mock.patch.object(
      breakpad_file_extractor, '_RunDumpSyms', side_effect=fake_dump_syms
    ):
      symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    self.assertEqual(
      sorted(os.listdir(self.options.breakpad_output_dir)),
      ['0A07AB57.breakpad', '892E74C4.breakpad'],
    )
    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_build_dir)
    shutil.rmtree(self.options.breakpad_output_dir)

  def testSymbolizerSeesPerfettoBreakpadNames(self):
    # Perfetto must find each breakpad file under its standard name and, for
    # 33-char Chrome module IDs, under the hex encoding of the ID text. The
    # breakpad dir must not change.
    self.options.local_breakpad_dir = tempfile.mkdtemp()
    chrome_name = '892E74C4A50E94B357E39FD6A360C84D0.breakpad'
    perfetto_name = (
      '383932453734433441353045393442333537453339464436413336304338344430'
      '.breakpad'
    )
    other_name = '34984AB4EF948D.breakpad'
    for name in (chrome_name, other_name):
      open(os.path.join(self.options.local_breakpad_dir, name), 'w').close()
    symbol_dirs = []

    def run_symbolizer(cmd, env, stdout):
      symbol_dirs.append(env['BREAKPAD_SYMBOL_DIR'])
      self.assertCountEqual(
        os.listdir(symbol_dirs[0]), [chrome_name, perfetto_name, other_name]
      )
      self.side_effect(cmd, env, stdout)

    symbolize_trace._RunSymbolizer = mock.MagicMock(side_effect=run_symbolizer)
    symbolize_trace.SymbolizeTrace(self.trace_file, self.options)

    # The temp dir is deleted and the breakpad dir is unchanged.
    self.assertFalse(os.path.exists(symbol_dirs[0]))
    self.assertCountEqual(
      os.listdir(self.options.local_breakpad_dir), [chrome_name, other_name]
    )

    os.remove(self.options.output_file)
    shutil.rmtree(self.options.local_breakpad_dir)


if __name__ == '__main__':
  unittest.main()

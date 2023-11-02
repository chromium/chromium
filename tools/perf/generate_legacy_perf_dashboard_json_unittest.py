#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import os
import unittest

import six

import generate_legacy_perf_dashboard_json

class LegacyResultsProcessorUnittest(unittest.TestCase):
  def setUp(self):
    """Set up for all test method of each test method below."""
    super(LegacyResultsProcessorUnittest, self).setUp()
    if six.PY2:
      self.data_directory = os.path.join(
          os.path.dirname(os.path.abspath(__file__)), 'testdata')
    else:
      self.data_directory = os.path.join(
          os.path.dirname(os.path.abspath(__file__)), 'testdata', 'python3')

  def _ConstructDefaultProcessor(self):
    """Creates a LegacyResultsProcessor instance.

    Returns:
      An instance of LegacyResultsProcessor class
    """
    return generate_legacy_perf_dashboard_json.LegacyResultsProcessor()

  def _ProcessLog(self, log_processor, logfile):  # pylint: disable=R0201
    """Reads in a input log file and processes it.

    This changes the state of the log processor object; the output is stored
    in the object and can be gotten using the PerformanceLogs() method.

    Args:
      log_processor: An PerformanceLogProcessor instance.
      logfile: File name of an input performance results log file.
    """
    for line in open(os.path.join(self.data_directory, logfile)):
      log_processor.ProcessLine(line)

  def _CheckFileExistsWithData(self, logs, graph):
    """Asserts that |graph| exists in the |logs| dict and is non-empty."""
    self.assertTrue(graph in logs, 'File %s was not output.' % graph)
    self.assertTrue(logs[graph], 'File %s did not contain data.' % graph)

  def _ConstructParseAndCheckLogfiles(self, inputfiles, graphs):
    """Uses a log processor to process the given input files.

    Args:
      inputfiles: A list of input performance results log file names.
      logfiles: List of expected output ".dat" file names.

    Returns:
      A dictionary mapping output file name to output file lines.
    """
    parser = self._ConstructDefaultProcessor()
    for inputfile in inputfiles:
      self._ProcessLog(parser, inputfile)

    logs = json.loads(parser.GenerateGraphJson())
    for graph in graphs:
      self._CheckFileExistsWithData(logs, graph)

    return logs

  def _ConstructParseAndCheckJSON(
      self, inputfiles, logfiles, graphs):
    """Processes input with a log processor and checks against expectations.

    Args:
      inputfiles: A list of input performance result log file names.
      logfiles: A list of expected output ".dat" file names.
      subdir: Subdirectory containing expected output files.
      log_processor_class: A log processor class.
    """
    logs = self._ConstructParseAndCheckLogfiles(inputfiles, graphs)
    index = 0
    for filename in logfiles:
      graph_name = graphs[index]
      actual = logs[graph_name]
      path = os.path.join(self.data_directory, filename)
      expected = json.load(open(path))
      self.assertEqual(expected, actual, 'JSON data in %s did not match '
          'expectations.' % filename)

      index += 1


  def testSummary(self):
    graphs = ['commit_charge',
        'ws_final_total', 'vm_final_browser', 'vm_final_total',
        'ws_final_browser', 'processes', 'artificial_graph']
    # Tests the output of "summary" files, which contain per-graph data.
    input_files = ['graphing_processor.log']
    output_files = ['%s-summary.dat' % graph for graph in graphs]

    self._ConstructParseAndCheckJSON(input_files, output_files, graphs)


if __name__ == '__main__':
  unittest.main()

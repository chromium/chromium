# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import six

from telemetry.page import shared_page_state
from telemetry.core import optparse_argparse_migration as oam

from contrib.cluster_telemetry import rasterize_and_record_micro_ct
from contrib.cluster_telemetry import skpicture_printer


class MockErrorParser(object):

  def __init__(self):
    self.err_msg = None

  def error(self, err_msg):
    self.err_msg = err_msg


class CTBenchmarks(unittest.TestCase):

  def setUp(self):
    self.ct_benchmarks = [
        rasterize_and_record_micro_ct.RasterizeAndRecordMicroCT(),
        skpicture_printer.SkpicturePrinterCT(),
    ]
    self.shared_page_state_class = shared_page_state.SharedMobilePageState
    self.archive_data_file = '/b/test'
    self.urls_list = 'http://test1.com,http://test2.com,http://test3.net'
    self.mock_parser = MockErrorParser()

  def testCTBenchmarks(self):
    for benchmark in self.ct_benchmarks:
      parser = oam.CreateFromOptparseInputs()
      parser.user_agent = 'mobile'
      parser.archive_data_file = self.archive_data_file
      parser.urls_list = self.urls_list

      benchmark.AddBenchmarkCommandLineArgs(parser)
      benchmark.ProcessCommandLineArgs(None, parser)
      ct_page_set = benchmark.CreateStorySet(parser)

      self.assertEqual(len(self.urls_list.split(',')), len(ct_page_set.stories))
      self.assertEqual(self.archive_data_file, ct_page_set.archive_data_file)
      for i in range(len(self.urls_list.split(','))):
        url = self.urls_list.split(',')[i]
        story = ct_page_set.stories[i]
        self.assertEqual(url, story.url)
        self.assertEqual(self.shared_page_state_class, story.shared_state_class)
        self.assertEqual(self.archive_data_file, story.archive_data_file)

  def testCTBenchmarks_wrongAgent(self):
    for benchmark in self.ct_benchmarks:
      parser = oam.CreateFromOptparseInputs()
      parser.user_agent = 'mobileeeeee'
      parser.archive_data_file = self.archive_data_file
      parser.urls_list = self.urls_list

      benchmark.AddBenchmarkCommandLineArgs(parser)
      benchmark.ProcessCommandLineArgs(None, parser)
      try:
        benchmark.CreateStorySet(parser)
        self.fail('Expected ValueError')
      except ValueError as e:
        self.assertEqual('user_agent mobileeeeee is unrecognized', str(e))

  def testCTBenchmarks_missingDataFile(self):
    for benchmark in self.ct_benchmarks:
      parser = oam.CreateFromOptparseInputs()
      parser.user_agent = 'mobile'
      parser.urls_list = self.urls_list
      parser.use_live_sites = False
      benchmark.AddBenchmarkCommandLineArgs(parser)

      # Should fail due to missing archive_data_file.
      try:
        benchmark.ProcessCommandLineArgs(None, parser)
        self.fail('Expected AttributeError')
      except AttributeError as e:
        if six.PY2:
          expected_error = (
              "ArgumentParser instance has no attribute 'archive_data_file'")
          actual_error = e.message
        else:
          expected_error = (
              "'ArgumentParser' object has no attribute 'archive_data_file'")
          actual_error = str(e)
        self.assertEqual(actual_error, expected_error)

      # Now add an empty archive_data_file.
      parser.archive_data_file = ''
      benchmark.ProcessCommandLineArgs(self.mock_parser, parser)
      self.assertEqual('Please specify --archive-data-file.',
                       self.mock_parser.err_msg)

  def testCTBenchmarks_missingDataFileUseLiveSites(self):
    for benchmark in self.ct_benchmarks:
      parser = oam.CreateFromOptparseInputs()
      parser.user_agent = 'mobile'
      parser.urls_list = self.urls_list
      parser.use_live_sites = True
      parser.archive_data_file = None
      benchmark.AddBenchmarkCommandLineArgs(parser)

      # Should pass.
      benchmark.ProcessCommandLineArgs(self.mock_parser, parser)
      self.assertIsNone(self.mock_parser.err_msg)

  def testCTBenchmarks_missingUrlsList(self):
    for benchmark in self.ct_benchmarks:
      parser = oam.CreateFromOptparseInputs()
      parser.user_agent = 'mobile'
      parser.archive_data_file = self.archive_data_file
      benchmark.AddBenchmarkCommandLineArgs(parser)

      # Should fail due to missing urls_list.
      try:
        benchmark.ProcessCommandLineArgs(None, parser)
        self.fail('Expected AttributeError')
      except AttributeError as e:
        if six.PY2:
          self.assertEqual(
              "ArgumentParser instance has no attribute 'urls_list'", str(e))
        else:
          self.assertEqual(
              "'ArgumentParser' object has no attribute 'urls_list'", str(e))

      # Now add an empty urls_list.
      parser.urls_list = ''
      benchmark.ProcessCommandLineArgs(self.mock_parser, parser)
      self.assertEqual('Please specify --urls-list.', self.mock_parser.err_msg)

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for results_processor.

These tests mostly test that argument parsing and processing work as expected.
They mock out accesses to the operating system, so no files are actually read
nor written.
"""

import datetime
import posixpath
import re
import unittest
from unittest import mock

from core.results_processor import command_line


# To easily mock module level symbols within the command_line module.
def module(symbol):
  return 'core.results_processor.command_line.' + symbol


class ProcessOptionsTestCase(unittest.TestCase):
  def setUp(self):
    self.standalone = False

    # Mock os module within results_processor so path manipulations do not
    # depend on the file system of the test environment.
    mock_os = mock.patch(module('os')).start()

    def realpath(path):
      return posixpath.normpath(posixpath.join(mock_os.getcwd(), path))

    def expanduser(path):
      return re.sub(r'~', '/path/to/home', path)

    mock_os.getcwd.return_value = '/path/to/curdir'
    mock_os.path.realpath.side_effect = realpath
    mock_os.path.expanduser.side_effect = expanduser
    mock_os.path.dirname.side_effect = posixpath.dirname
    mock_os.path.join.side_effect = posixpath.join
    mock.patch(module('_DefaultOutputDir'),
               return_value='/path/to/output_dir').start()

  def tearDown(self):
    mock.patch.stopall()

  def ParseArgs(self, args):
    parser = command_line.ArgumentParser(standalone=self.standalone)
    options = parser.parse_args(args)
    command_line.ProcessOptions(options)
    return options


class TestProcessOptions(ProcessOptionsTestCase):
  def testOutputDir_default(self):
    options = self.ParseArgs([])
    self.assertEqual(options.output_dir, '/path/to/output_dir')

  def testOutputDir_homeDir(self):
    options = self.ParseArgs(['--output-dir', '~/my_outputs'])
    self.assertEqual(options.output_dir, '/path/to/home/my_outputs')

  def testOutputDir_relPath(self):
    options = self.ParseArgs(['--output-dir', 'my_outputs'])
    self.assertEqual(options.output_dir, '/path/to/curdir/my_outputs')

  def testOutputDir_absPath(self):
    options = self.ParseArgs(['--output-dir', '/path/to/somewhere/else'])
    self.assertEqual(options.output_dir, '/path/to/somewhere/else')

  @mock.patch(module('datetime'))
  def testIntermediateDir_default(self, mock_datetime):
    mock_datetime.datetime.utcnow.return_value = (
        datetime.datetime(2015, 10, 21, 7, 28))
    options = self.ParseArgs(['--output-dir', '/output'])
    self.assertEqual(options.intermediate_dir,
                     '/output/artifacts/run_20151021T072800Z')

  @mock.patch(module('datetime'))
  def testIntermediateDir_withResultsLabel(self, mock_datetime):
    mock_datetime.datetime.utcnow.return_value = (
        datetime.datetime(2015, 10, 21, 7, 28))
    options = self.ParseArgs(
        ['--output-dir', '/output', '--results-label', 'test my feature'])
    self.assertEqual(options.intermediate_dir,
                     '/output/artifacts/run_20151021T072800Z')

  def testUploadBucket_noUploadResults(self):
    options = self.ParseArgs([])
    self.assertFalse(options.upload_results)
    self.assertIsNone(options.upload_bucket)

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToDefaultBucket(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {'output': 'default-bucket'}
    options = self.ParseArgs(['--upload-results'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'default-bucket')

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToBucket(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {'output': 'default-bucket'}
    options = self.ParseArgs(
        ['--upload-results', '--upload-bucket', 'my_bucket'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'my_bucket')

  @mock.patch(module('cloud_storage'))
  def testUploadBucket_uploadResultsToAlias(self, mock_storage):
    mock_storage.BUCKET_ALIASES = {
        'output': 'default-bucket', 'special': 'some-special-bucket'}
    options = self.ParseArgs(
        ['--upload-results', '--upload-bucket', 'special'])
    self.assertTrue(options.upload_results)
    self.assertEqual(options.upload_bucket, 'some-special-bucket')

  def testDefaultOutputFormat(self):
    options = self.ParseArgs([])
    self.assertEqual(options.output_formats, ['html'])

  def testUnkownOutputFormatRaises(self):
    with self.assertRaises(SystemExit):
      self.ParseArgs(['--output-format', 'unknown'])

  def testNoDuplicateOutputFormats(self):
    options = self.ParseArgs(
        ['--output-format', 'html', '--output-format', 'csv',
         '--output-format', 'html', '--output-format', 'csv'])
    self.assertEqual(options.output_formats, ['csv', 'html'])


class StandaloneTestProcessOptions(ProcessOptionsTestCase):
  def setUp(self):
    super(StandaloneTestProcessOptions, self).setUp()
    self.standalone = True

  def testOutputFormatRequired(self):
    with self.assertRaises(SystemExit):
      self.ParseArgs([])

  def testIntermediateDirRequired(self):
    with self.assertRaises(SystemExit):
      self.ParseArgs(['--output-format', 'json-test-results'])

  def testSuccessful(self):
    options = self.ParseArgs(
        ['--output-format', 'json-test-results',
         '--intermediate-dir', 'some_dir'])
    self.assertEqual(options.output_formats, ['json-test-results'])
    self.assertEqual(options.intermediate_dir, '/path/to/curdir/some_dir')
    self.assertEqual(options.output_dir, '/path/to/output_dir')

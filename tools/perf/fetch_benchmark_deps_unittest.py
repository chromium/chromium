# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import tempfile
import unittest

import mock  # pylint: disable=import-error

from py_utils import cloud_storage
from telemetry.internal.util import binary_manager
from telemetry.wpr import archive_info

from core import path_util
import fetch_benchmark_deps


def NormPaths(paths):
  return sorted([os.path.normcase(p) for p in paths.splitlines()])


class FetchBenchmarkDepsUnittest(unittest.TestCase):
  """The test guards fetch_benchmark_deps.

  It assumes the following telemetry APIs always success:
  telemetry.wpr.archive_info.WprArchiveInfo.DownloadArchivesIfNeeded
  py_utils.cloud_storage.GetFilesInDirectoryIfChanged
  """

  @mock.patch.object(fetch_benchmark_deps, 'FetchDepsForCrossbench')
  def testFetchWPRs(self, _):
    test_name = 'system_health.common_desktop'
    deps_fd, deps_path = tempfile.mkstemp()
    args = [test_name, '--output-deps=%s' % deps_path]
    with mock.patch.object(archive_info.WprArchiveInfo,
        'DownloadArchivesIfNeeded', autospec=True) as mock_download:
      with mock.patch('py_utils.cloud_storage'
                      '.GetFilesInDirectoryIfChanged') as mock_get:
        mock_download.return_value = True
        mock_get.GetFilesInDirectoryIfChanged.return_value = True
        fetch_benchmark_deps.main(args)
        self.assertEqual(
            # pylint: disable=protected-access
            os.path.normpath(mock_download.call_args[0][0]._file_path),
            os.path.join(path_util.GetPerfStorySetsDir(), 'data',
            'system_health_desktop.json'))
        # This benchmark doesn't use any static local files.
        self.assertFalse(mock_get.called)

    # Gets json content and remove the temp json file.
    with open(deps_path) as deps_file:
      deps = json.loads(deps_file.read())
    os.close(deps_fd)
    os.remove(deps_path)

    # Checks fetch_benchmark_deps.py output.
    output_count = 0
    for dep in deps[test_name]:
      fullpath = os.path.join(path_util.GetChromiumSrcDir(), dep)
      sha1path = fullpath + '.sha1'
      self.assertTrue(os.path.isfile(sha1path))
      output_count += 1
    self.assertTrue(output_count > 0)

  @mock.patch.object(fetch_benchmark_deps, 'FetchDepsForCrossbench')
  def testFetchServingDirs(self, _):
    args = ['media.desktop']
    with mock.patch.object(archive_info.WprArchiveInfo,
        'DownloadArchivesIfNeeded', autospec=True) as mock_download:
      with mock.patch('py_utils.cloud_storage'
                      '.GetFilesInDirectoryIfChanged') as mock_get:
        mock_download.return_value = True
        mock_get.GetFilesInDirectoryIfChanged.return_value = True
        fetch_benchmark_deps.main(args)
        # This benchmark doesn't use any archive files.
        self.assertFalse(mock_download.called)
        mock_get.assert_called_once_with(
            os.path.join(path_util.GetPerfStorySetsDir(), 'media_cases'),
            cloud_storage.PARTNER_BUCKET)

  @mock.patch.object(binary_manager, 'InitDependencyManager')
  @mock.patch.object(binary_manager, 'FetchBinaryDependencies')
  def testFetchDepsForCrossbench(self, mock_init_dependency_manager,
                                 mock_fetch_binary_depdencies):
    with mock.patch.object(archive_info.WprArchiveInfo,
                           'DownloadArchivesIfNeeded',
                           autospec=True) as mock_download:
      mock_download.return_value = True

      fetch_benchmark_deps.FetchDepsForCrossbench()

      self.assertTrue(mock_download.called)
      self.assertTrue(mock_init_dependency_manager.called)
      self.assertTrue(mock_fetch_binary_depdencies.called)

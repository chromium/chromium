# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
import urllib.request
from unittest.mock import (ANY, Mock, MagicMock, mock_open, patch, call,
                           PropertyMock)

bisect_builds = __import__('bisect-builds')

if 'NO_MOCK_SERVER' not in os.environ:
  maybe_patch = patch
else:
  # SetupEnvironment for gsutil to connect to real server.
  options = bisect_builds.ParseCommandLine(['-a', 'linux64', '-g', '1'])
  bisect_builds.SetupEnvironment(options)
  bisect_builds.SetupAndroidEnvironment()

  # Mock object that always wraps for the spec.
  # This will pass the call through and ignore the return_value and side_effect.
  class WrappedMock(MagicMock):

    def __init__(self,
                 spec=None,
                 return_value=None,
                 side_effect=None,
                 *args,
                 **kwargs):
      wraps = kwargs.pop('wraps', spec)
      super().__init__(spec, *args, **kwargs, wraps=wraps)

  maybe_patch = functools.partial(patch, spec=True, new_callable=WrappedMock)
  maybe_patch.object = functools.partial(patch.object,
                                         spec=True,
                                         new_callable=WrappedMock)


class BisectTestCase(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    # Patch the name pattern for pkgutil to accept "bisect-builds" as module
    # name.
    if sys.version_info[:2] > (3, 8):
      dotted_words = r'(?!\d)([\w-]+)(\.(?!\d)(\w+))*'
      name_pattern = re.compile(
          f'^(?P<pkg>{dotted_words})'
          f'(?P<cln>:(?P<obj>{dotted_words})?)?$', re.UNICODE)
      cls.name_pattern_patcher = patch('pkgutil._NAME_PATTERN', name_pattern)
      cls.name_pattern_patcher.start()

    # patch cache filename to prevent pollute working dir.
    fd, cls.tmp_cache_file = tempfile.mkstemp(suffix='.json')
    os.close(fd)
    cls.cache_filename_patcher = patch(
        'bisect-builds.ArchiveBuild._rev_list_cache_filename',
        new=PropertyMock(return_value=cls.tmp_cache_file))
    cls.cache_filename_patcher.start()

  @classmethod
  def tearDownClass(cls):
    if sys.version_info[:2] > (3, 8):
      cls.name_pattern_patcher.stop()
    cls.cache_filename_patcher.stop()
    os.unlink(cls.tmp_cache_file)


class BisectTest(BisectTestCase):

  max_rev = 10000

  def setUp(self):
    self.patchers = []
    self.patchers.append(patch('bisect-builds.DownloadJob._fetch'))
    self.patchers.append(
        patch('bisect-builds.ArchiveBuild.run_revision',
              return_value=(0, '', '')))
    self.patchers.append(
        patch('bisect-builds.SnapshotBuild._get_rev_list',
              return_value=range(self.max_rev)))
    for each in self.patchers:
      each.start()

  def tearDown(self):
    for each in self.patchers:
      each.stop()

  def bisect(self, good_rev, bad_rev, evaluate, num_runs=1):
    options = bisect_builds.ParseCommandLine([
        '-a', 'linux64', '-g',
        str(good_rev), '-b',
        str(bad_rev), '--times',
        str(num_runs), '--no-local-cache'
    ])
    archive_build = bisect_builds.create_archive_build(options)
    (minrev, maxrev) = bisect_builds.Bisect(archive_build=archive_build,
                                            evaluate=evaluate,
                                            try_args=options.args)
    return (minrev, maxrev)

  @patch('builtins.print')
  def testBisectConsistentAnswer(self, mock_print):

    def get_steps():
      steps = []
      for call in mock_print.call_args_list:
        if call.args and call.args[0].startswith('You have'):
          steps.append(int(re.search(r'(\d+) steps', call.args[0])[1]))
      return steps

    self.assertEqual(self.bisect(1000, 100, lambda *args: 'g'), (100, 101))
    self.assertSequenceEqual(get_steps(), range(10, 1, -1))

    mock_print.reset_mock()
    self.assertEqual(self.bisect(100, 1000, lambda *args: 'b'), (100, 101))
    self.assertSequenceEqual(get_steps(), range(10, 0, -1))

    mock_print.reset_mock()
    self.assertEqual(self.bisect(2000, 200, lambda *args: 'b'), (1999, 2000))
    self.assertSequenceEqual(get_steps(), range(11, 0, -1))

    mock_print.reset_mock()
    self.assertEqual(self.bisect(200, 2000, lambda *args: 'g'), (1999, 2000))
    self.assertSequenceEqual(get_steps(), range(11, 1, -1))

  @patch('bisect-builds.ArchiveBuild.run_revision', return_value=(0, '', ''))
  def test_bisect_should_retry(self, mock_run_revision):
    evaluator = Mock(side_effect='rgrgrbr')
    self.assertEqual(self.bisect(9, 1, evaluator), (2, 3))
    tested_revisions = [c.args[0] for c in evaluator.call_args_list]
    self.assertEqual(tested_revisions, [5, 5, 3, 3, 2, 2])
    self.assertEqual(mock_run_revision.call_count, 6)

    evaluator = Mock(side_effect='rgrrrgrbr')
    self.assertEqual(self.bisect(1, 10, evaluator), (8, 9))
    tested_revisions = [c.args[0] for c in evaluator.call_args_list]
    self.assertEqual(tested_revisions, [6, 6, 8, 8, 8, 8, 9, 9])

  def test_bisect_should_unknown(self):
    evaluator = Mock(side_effect='uuuggggg')
    self.assertEqual(self.bisect(9, 1, evaluator), (1, 2))
    tested_revisions = [c.args[0] for c in evaluator.call_args_list]
    self.assertEqual(tested_revisions, [5, 3, 6, 7, 2])

    evaluator = Mock(side_effect='uuugggggg')
    self.assertEqual(self.bisect(1, 9, evaluator), (8, 9))
    tested_revisions = [c.args[0] for c in evaluator.call_args_list]
    self.assertEqual(tested_revisions, [5, 7, 4, 3, 8])

  def test_bisect_should_quit(self):
    evaluator = Mock(side_effect=SystemExit())
    with self.assertRaises(SystemExit):
      self.assertEqual(self.bisect(9, 1, evaluator), (None, None))

  def test_edge_cases(self):
    with self.assertRaises(bisect_builds.BisectException):
      self.assertEqual(self.bisect(1, 1, Mock()), (1, 1))
    self.assertEqual(self.bisect(2, 1, Mock()), (1, 2))
    self.assertEqual(self.bisect(1, 2, Mock()), (1, 2))


class DownloadJobTest(BisectTestCase):

  @patch('bisect-builds.gsutil_download')
  def test_fetch_gsutil(self, mock_gsutil_download):
    fetch = bisect_builds.DownloadJob('gs://some-file.zip', 123)
    fetch.start()
    fetch.wait_for()
    mock_gsutil_download.assert_called_once()

  @patch('urllib.request.urlretrieve')
  def test_fetch_http(self, mock_urlretrieve):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    fetch.start()
    fetch.wait_for()
    mock_urlretrieve.assert_called_once()

  @patch('tempfile.mkstemp', return_value=(321, 'some-file.zip'))
  @patch('urllib.request.urlretrieve')
  @patch('os.close')
  @patch('os.unlink')
  def test_should_del(self, mock_unlink, mock_close, mock_urlretrieve,
                      mock_mkstemp):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    fetch.start().wait_for()
    fetch.stop()
    mock_mkstemp.assert_called_once()
    mock_close.assert_called_once()
    mock_urlretrieve.assert_called_once()
    mock_unlink.assert_called_with('some-file.zip')

  @patch('urllib.request.urlretrieve')
  def test_stop_wait_for_should_be_able_to_reenter(self, mock_urlretrieve):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    fetch.start()
    fetch.wait_for()
    fetch.wait_for()
    fetch.stop()
    fetch.stop()

  @patch('tempfile.mkstemp',
         side_effect=[(321, 'some-file.apks'), (123, 'file2.apk')])
  @patch('bisect-builds.gsutil_download')
  @patch('os.close')
  @patch('os.unlink')
  def test_should_support_multiple_files(self, mock_unlink, mock_close,
                                         mock_gsutil, mock_mkstemp):
    urls = {
        'trichrome':
        ('gs://chrome-unsigned/android-B0urB0N/129.0.6626.0/high-arm_64/'
         'TrichromeChromeGoogle6432Stable.apks'),
        'trichrome_library':
        ('gs://chrome-unsigned/android-B0urB0N/129.0.6626.0/high-arm_64/'
         'TrichromeLibraryGoogle6432Stable.apk'),
    }
    fetch = bisect_builds.DownloadJob(urls, 123)
    result = fetch.start().wait_for()
    fetch.stop()
    self.assertDictEqual(result, {
        'trichrome': 'some-file.apks',
        'trichrome_library': 'file2.apk',
    })
    self.assertEqual(mock_mkstemp.call_count, 2)
    self.assertEqual(mock_close.call_count, 2)
    mock_unlink.assert_has_calls([call('some-file.apks'), call('file2.apk')])
    self.assertEqual(mock_gsutil.call_count, 2)


  @patch(
      "urllib.request.urlopen",
      side_effect=urllib.request.HTTPError('url', 404, 'Not Found', None, None),
  )
  @patch('subprocess.Popen', spec=subprocess.Popen)
  @patch('bisect-builds.GSUTILS_PATH', new='/some/path')
  def test_download_failure_should_raised(self, mock_Popen, mock_urlopen):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    with self.assertRaises(urllib.request.HTTPError):
      fetch.start().wait_for()

    mock_Popen.return_value.communicate.return_value = (b'', b'status=403')
    mock_Popen.return_value.returncode = 1
    fetch = bisect_builds.DownloadJob('gs://some-file.zip', 123)
    with self.assertRaises(bisect_builds.BisectException):
      fetch.start().wait_for()


class ArchiveBuildTest(BisectTestCase):

  def setUp(self):
    self.patcher = patch.multiple(
        bisect_builds.ArchiveBuild,
        __abstractmethods__=set(),
        build_type='release',
        _get_rev_list=Mock(return_value=list(map(str, range(10)))),
        _rev_list_cache_key='abc')
    self.patcher.start()

  def tearDown(self):
    self.patcher.stop()

  def create_build(self, *args):
    args = ['-a', 'linux64', '-g', '0', '-b', '9', *args]
    options = bisect_builds.ParseCommandLine(args)
    return bisect_builds.ArchiveBuild(options)

  def test_cache_should_not_work_if_not_enabled(self):
    build = self.create_build('--no-local-cache')
    self.assertFalse(build.use_local_cache)
    with patch('builtins.open') as m:
      self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
      bisect_builds.ArchiveBuild._get_rev_list.assert_called_once()
      m.assert_not_called()

  def test_cache_should_save_and_load(self):
    build = self.create_build()
    self.assertTrue(build.use_local_cache)
    # Load the non-existent cache and write to it.
    cached_data = []
    # The cache file would be opened 3 times:
    #   1. read by _load_rev_list_cache
    #   2. read by _save_rev_list_cache for existing cache
    #   3. write by _save_rev_list_cache
    write_mock = MagicMock()
    write_mock.__enter__().write.side_effect = lambda d: cached_data.append(d)
    with patch('builtins.open',
               side_effect=[FileNotFoundError, FileNotFoundError, write_mock]):
      self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
      bisect_builds.ArchiveBuild._get_rev_list.assert_called_once()
    cached_json = json.loads(''.join(cached_data))
    self.assertDictEqual(cached_json, {'abc': [str(x) for x in range(10)]})
    # Load cache with cached data.
    build = self.create_build('--use-local-cache')
    bisect_builds.ArchiveBuild._get_rev_list.reset_mock()
    with patch('builtins.open', mock_open(read_data=''.join(cached_data))):
      self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
      bisect_builds.ArchiveBuild._get_rev_list.assert_not_called()

  @patch.object(bisect_builds.ArchiveBuild, '_load_rev_list_cache')
  @patch.object(bisect_builds.ArchiveBuild, '_save_rev_list_cache')
  @patch.object(bisect_builds.ArchiveBuild,
                '_get_rev_list',
                return_value=[str(x) for x in range(10)])
  def test_should_request_partial_rev_list(self, mock_get_rev_list,
                                           mock_save_rev_list_cache,
                                           mock_load_rev_list_cache):
    build = self.create_build('--no-local-cache')
    # missing latest
    mock_load_rev_list_cache.return_value = [str(x) for x in range(5)]
    self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
    mock_get_rev_list.assert_called_with('4', '9')
    # missing old and latest
    mock_load_rev_list_cache.return_value = [str(x) for x in range(1, 5)]
    self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
    mock_get_rev_list.assert_called_with('0', '9')
    # missing old
    mock_load_rev_list_cache.return_value = [str(x) for x in range(3, 10)]
    self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
    mock_get_rev_list.assert_called_with('0', '3')
    # no intersect
    mock_load_rev_list_cache.return_value = ['c', 'd', 'e']
    self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
    mock_save_rev_list_cache.assert_called_with([str(x) for x in range(10)] +
                                                ['c', 'd', 'e'])
    mock_get_rev_list.assert_called_with('0', 'c')

  @patch.object(bisect_builds.ArchiveBuild, '_get_rev_list', return_value=[])
  def test_should_raise_error_when_no_rev_list(self, mock_get_rev_list):
    build = self.create_build('--no-local-cache')
    with self.assertRaises(bisect_builds.BisectException):
      build.get_rev_list()
    mock_get_rev_list.assert_any_call('0', '9')
    mock_get_rev_list.assert_any_call()

  @unittest.skipIf('NO_MOCK_SERVER' not in os.environ,
                   'The test is to ensure NO_MOCK_SERVER working correctly')
  @maybe_patch('bisect-builds.GetRevisionFromVersion', return_value=123)
  def test_no_mock(self, mock_GetRevisionFromVersion):
    self.assertEqual(bisect_builds.GetRevisionFromVersion('127.0.6533.74'),
                     1313161)
    mock_GetRevisionFromVersion.assert_called()

  @patch('bisect-builds.ArchiveBuild._install_revision')
  @patch('bisect-builds.ArchiveBuild._launch_revision',
         return_value=(1, '', ''))
  def test_run_revision_should_return_early(self, mock_launch_revision,
                                            mock_install_revision):
    build = self.create_build()
    build.run_revision('', '', [])
    mock_launch_revision.assert_called_once()

  @patch('bisect-builds.ArchiveBuild._install_revision')
  @patch('bisect-builds.ArchiveBuild._launch_revision',
         return_value=(0, '', ''))
  def test_run_revision_should_do_all_runs(self, mock_launch_revision,
                                           mock_install_revision):
    build = self.create_build('--time', '10')
    build.run_revision('', '', [])
    self.assertEqual(mock_launch_revision.call_count, 10)

  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('glob.glob', return_value=['temp-dir/linux64/chrome'])
  @patch('os.path.abspath', return_value='/tmp/temp-dir/linux64/chrome')
  def test_install_revision_should_unzip_and_search_executable(
      self, mock_abspath, mock_glob, mock_UnzipFilenameToDir):
    build = self.create_build()
    self.assertEqual(build._install_revision('some-file.zip', 'temp-dir'),
                     {'chrome': '/tmp/temp-dir/linux64/chrome'})
    mock_UnzipFilenameToDir.assert_called_once_with('some-file.zip', 'temp-dir')
    mock_glob.assert_called_once_with('temp-dir/*/chrome')
    mock_abspath.assert_called_once_with('temp-dir/linux64/chrome')

  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('glob.glob',
         side_effect=[['temp-dir/chrome-linux64/chrome'],
                      ['temp-dir/chromedriver_linux64/chromedriver']])
  @patch('os.path.abspath',
         side_effect=[
             '/tmp/temp-dir/chrome-linux64/chrome',
             '/tmp/temp-dir/chromedriver_linux64/chromedriver'
         ])
  def test_install_chromedriver(self, mock_abspath, mock_glob,
                                mock_UnzipFilenameToDir):
    build = self.create_build('--chromedriver')
    self.assertEqual(
        build._install_revision(
            {
                'chrome': 'some-file.zip',
                'chromedriver': 'some-other-file.zip',
            }, 'temp-dir'),
        {
            'chrome': '/tmp/temp-dir/chrome-linux64/chrome',
            'chromedriver': '/tmp/temp-dir/chromedriver_linux64/chromedriver',
        })
    mock_UnzipFilenameToDir.assert_has_calls([
        call('some-file.zip', 'temp-dir'),
        call('some-other-file.zip', 'temp-dir'),
    ])
    mock_glob.assert_has_calls([
        call('temp-dir/*/chrome'),
        call('temp-dir/*/chromedriver'),
    ])
    mock_abspath.assert_has_calls([
        call('temp-dir/chrome-linux64/chrome'),
        call('temp-dir/chromedriver_linux64/chromedriver')
    ])

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_launch_revision_should_run_command(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    build = self.create_build()
    build._launch_revision('temp-dir', {'chrome': 'temp-dir/linux64/chrome'},
                           [])
    mock_Popen.assert_called_once_with(
        'temp-dir/linux64/chrome --user-data-dir=temp-dir/profile',
        cwd=None,
        shell=True,
        bufsize=-1,
        stdout=ANY,
        stderr=ANY)

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_command_replacement(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    build = self.create_build(
        '--chromedriver', '-c',
        'CHROMEDRIVER=%d BROWSER_EXECUTABLE_PATH=%p pytest %a')
    build._launch_revision('/tmp', {
        'chrome': '/tmp/chrome',
        'chromedriver': '/tmp/chromedriver'
    }, ['--args', '--args2="word 1"', 'word 2'])
    mock_Popen.assert_called_once_with(
        'CHROMEDRIVER=/tmp/chromedriver BROWSER_EXECUTABLE_PATH=/tmp/chrome '
        'pytest --user-data-dir=/tmp/profile --args \'--args2="word 1"\' '
        '\'word 2\'',
        cwd=None,
        shell=True,
        bufsize=-1,
        stdout=ANY,
        stderr=ANY)


class ReleaseBuildTest(BisectTestCase):

  def test_should_look_up_path_context(self):
    options = bisect_builds.ParseCommandLine(
        ['-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88'])
    self.assertEqual(options.archive, 'linux64')
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    self.assertEqual(build.binary_name, 'chrome')
    self.assertEqual(build.listing_platform_dir, 'linux64/')
    self.assertEqual(build.archive_name, 'chrome-linux64.zip')
    self.assertEqual(build.archive_extract_dir, 'chrome-linux64')

  @maybe_patch(
      'bisect-builds.GsutilList',
      return_value=[
          'gs://chrome-unsigned/desktop-5c0tCh/%s/linux64/chrome-linux64.zip' %
          x for x in ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76']
      ])
  def test_get_rev_list(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.76',
        '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    self.assertEqual(build.get_rev_list(),
                     ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76'])
    mock_GsutilList.assert_any_call('gs://chrome-unsigned/desktop-5c0tCh')
    mock_GsutilList.assert_any_call(*[
        'gs://chrome-unsigned/desktop-5c0tCh/%s/linux64/chrome-linux64.zip' % x
        for x in ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76']
    ],
                                    ignore_fail=True)
    self.assertEqual(mock_GsutilList.call_count, 2)

  @patch('bisect-builds.GsutilList',
         return_value=['127.0.6533.74', '127.0.6533.75', '127.0.6533.76'])
  def test_should_save_and_load_cache(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.77',
        '--use-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    # Load the non-existent cache and write to it.
    cached_data = []
    write_mock = MagicMock()
    write_mock.__enter__().write.side_effect = lambda d: cached_data.append(d)
    with patch('builtins.open',
               side_effect=[FileNotFoundError, FileNotFoundError, write_mock]):
      self.assertEqual(build.get_rev_list(),
                       ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76'])
      mock_GsutilList.assert_called()
    cached_json = json.loads(''.join(cached_data))
    self.assertDictEqual(
        cached_json, {
            build._rev_list_cache_key:
            ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76']
        })
    # Load cache with cached data and merge with new data
    mock_GsutilList.return_value = ['127.0.6533.76', '127.0.6533.77']
    build = bisect_builds.create_archive_build(options)
    with patch('builtins.open', mock_open(read_data=''.join(cached_data))):
      self.assertEqual(
          build.get_rev_list(),
          ['127.0.6533.74', '127.0.6533.75', '127.0.6533.76', '127.0.6533.77'])
    print(mock_GsutilList.call_args)
    mock_GsutilList.assert_any_call(
        'gs://chrome-unsigned/desktop-5c0tCh'
        '/127.0.6533.76/linux64/chrome-linux64.zip',
        'gs://chrome-unsigned/desktop-5c0tCh'
        '/127.0.6533.77/linux64/chrome-linux64.zip',
        ignore_fail=True)

  def test_get_download_url(self):
    options = bisect_builds.ParseCommandLine(
        ['-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.77'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    download_urls = build.get_download_url('127.0.6533.74')
    self.assertEqual(
        download_urls, 'gs://chrome-unsigned/desktop-5c0tCh'
        '/127.0.6533.74/linux64/chrome-linux64.zip')

    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.77',
        '--chromedriver'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    download_urls = build.get_download_url('127.0.6533.74')
    self.assertEqual(
        download_urls, {
            'chrome':
            'gs://chrome-unsigned/desktop-5c0tCh'
            '/127.0.6533.74/linux64/chrome-linux64.zip',
            'chromedriver':
            'gs://chrome-unsigned/desktop-5c0tCh'
            '/127.0.6533.74/linux64/chromedriver_linux64.zip',
        })

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, 'stdout', ''))
  def test_run_revision_with_real_zipfile(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.77',
        '--chromedriver', '-c', 'driver=%d prog=%p'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ReleaseBuild)
    download_job = build.get_download_job('127.0.6533.74')
    zip_file = download_job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(zip_file, tempdir, [])
    self.assertRegex(mock_run.call_args.args[0],
                     r'driver=.+/chromedriver prog=.+/chrome')


class ArchiveBuildWithCommitPositionTest(BisectTestCase):

  def setUp(self):
    patch.multiple(bisect_builds.ArchiveBuildWithCommitPosition,
                   __abstractmethods__=set(),
                   build_type='release').start()

  @maybe_patch('bisect-builds.GetRevisionFromVersion', return_value=1313161)
  @maybe_patch('bisect-builds.GetChromiumRevision', return_value=999999999)
  def test_should_convert_revision_as_commit_position(
      self, mock_GetChromiumRevision, mock_GetRevisionFromVersion):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '127.0.6533.74'])
    build = bisect_builds.ArchiveBuildWithCommitPosition(options)
    self.assertEqual(build.good_revision, 1313161)
    self.assertEqual(build.bad_revision, 999999999)
    mock_GetRevisionFromVersion.assert_called_once_with('127.0.6533.74')
    mock_GetChromiumRevision.assert_called()


class OfficialBuildTest(BisectTestCase):

  def test_should_lookup_path_context(self):
    options = bisect_builds.ParseCommandLine(
        ['-o', '-a', 'linux64', '-g', '0', '-b', '10'])
    self.assertEqual(options.archive, 'linux64')
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.OfficialBuild)
    self.assertEqual(build.binary_name, 'chrome')
    self.assertEqual(build.listing_platform_dir, 'linux-builder-perf/')
    self.assertEqual(build.archive_name, 'chrome-perf-linux.zip')
    self.assertEqual(build.archive_extract_dir, 'full-build-linux')

  @maybe_patch('bisect-builds.GsutilList',
               return_value=[
                   'full-build-linux_%d.zip' % x
                   for x in range(1313161, 1313164)
               ])
  def test_get_rev_list(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'linux64', '-g', '1313161', '-b', '1313163',
        '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.OfficialBuild)
    self.assertEqual(build.get_rev_list(), list(range(1313161, 1313164)))
    mock_GsutilList.assert_called_once_with(
        'gs://chrome-test-builds/official-by-commit/linux-builder-perf/')

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, 'stdout', ''))
  def test_run_revision_with_real_zipfile(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'linux64', '-g', '1313161', '-b', '1313163',
        '--chromedriver', '-c', 'driver=%d prog=%p'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.OfficialBuild)
    download_job = build.get_download_job(1334339)
    zip_file = download_job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(zip_file, tempdir, [])
    self.assertRegex(mock_run.call_args.args[0],
                     r'driver=.+/chromedriver prog=.+/chrome')

class SnapshotBuildTest(BisectTestCase):

  def test_should_lookup_path_context(self):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '0', '-b', '10'])
    self.assertEqual(options.archive, 'linux64')
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    self.assertEqual(build.binary_name, 'chrome')
    self.assertEqual(build.listing_platform_dir, 'Linux_x64/')
    self.assertEqual(build.archive_name, 'chrome-linux.zip')
    self.assertEqual(build.archive_extract_dir, 'chrome-linux')

  CommonDataXMLContent = '''<?xml version='1.0' encoding='UTF-8'?>
    <ListBucketResult xmlns='http://doc.s3.amazonaws.com/2006-03-01'>
      <Name>chromium-browser-snapshots</Name>
      <Prefix>Linux_x64/</Prefix>
      <Marker></Marker>
      <NextMarker></NextMarker>
      <Delimiter>/</Delimiter>
      <IsTruncated>true</IsTruncated>
      <CommonPrefixes>
        <Prefix>Linux_x64/1313161/</Prefix>
      </CommonPrefixes>
      <CommonPrefixes>
        <Prefix>Linux_x64/1313163/</Prefix>
      </CommonPrefixes>
      <CommonPrefixes>
        <Prefix>Linux_x64/1313185/</Prefix>
      </CommonPrefixes>
    </ListBucketResult>
  '''

  @maybe_patch('urllib.request.urlopen',
               return_value=io.StringIO(CommonDataXMLContent))
  @patch('bisect-builds.GetChromiumRevision', return_value=1313185)
  def test_get_rev_list(self, mock_GetChromiumRevision, mock_urlopen):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '1313161', '-b', '1313185', '--no-local-cache'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    rev_list = build.get_rev_list()
    mock_urlopen.assert_any_call(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
        '?delimiter=/&prefix=Linux_x64/&marker=Linux_x64/1313161')
    self.assertEqual(mock_urlopen.call_count, 1)
    self.assertEqual(rev_list, [1313161, 1313163, 1313185])

  @patch('bisect-builds.SnapshotBuild._fetch_and_parse',
         return_value=([int(s)
                        for s in sorted([str(x) for x in range(1, 11)])], None))
  def test_get_rev_list_should_start_from_a_marker(self, mock_fetch_and_parse):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '0', '-b', '9', '--no-local-cache'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    rev_list = build._get_rev_list(0, 9)
    self.assertEqual(rev_list, list(range(1, 10)))
    mock_fetch_and_parse.assert_called_once_with(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
        '?delimiter=/&prefix=Linux_x64/&marker=Linux_x64/0')
    mock_fetch_and_parse.reset_mock()
    rev_list = build._get_rev_list(1, 9)
    self.assertEqual(rev_list, list(range(1, 10)))
    mock_fetch_and_parse.assert_called_once_with(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
        '?delimiter=/&prefix=Linux_x64/&marker=Linux_x64/1')

  @patch('bisect-builds.SnapshotBuild._fetch_and_parse',
         return_value=([int(s)
                        for s in sorted([str(x) for x in range(1, 11)])], None))
  def test_get_rev_list_should_scan_all_pages(self, mock_fetch_and_parse):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '3', '-b', '11', '--no-local-cache'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    rev_list = build._get_rev_list(0, 11)
    self.assertEqual(sorted(rev_list), list(range(1, 11)))
    mock_fetch_and_parse.assert_called_once_with(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
        '?delimiter=/&prefix=Linux_x64/')

  def test_get_download_url(self):
    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '3', '-b', '11'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    download_urls = build.get_download_url(123)
    self.assertEqual(
        download_urls,
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots'
        '/Linux_x64/123/chrome-linux.zip',
    )

    options = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '3', '-b', '11', '--chromedriver'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    download_urls = build.get_download_url(123)
    self.assertDictEqual(
        download_urls, {
            'chrome':
            'http://commondatastorage.googleapis.com/chromium-browser-snapshots'
            '/Linux_x64/123/chrome-linux.zip',
            'chromedriver':
            'http://commondatastorage.googleapis.com/chromium-browser-snapshots'
            '/Linux_x64/123/chromedriver_linux64.zip'
        })

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, 'stdout', ''))
  def test_run_revision_with_real_zipfile(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-a', 'linux64', '-g', '1313161', '-b', '1313185', '--chromedriver',
        '-c', 'driver=%d prog=%p'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    download_job = build.get_download_job(1313161)
    zip_file = download_job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(zip_file, tempdir, [])
    self.assertRegex(mock_run.call_args.args[0],
                     r'driver=.+/chromedriver prog=.+/chrome')

  @patch('bisect-builds.GetChromiumRevision', return_value=1313185)
  def test_get_bad_revision(self, mock_GetChromiumRevision):
    options = bisect_builds.ParseCommandLine(['-a', 'linux64', '-g', '1313161'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    mock_GetChromiumRevision.assert_called_once_with(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots'
        '/Linux_x64/LAST_CHANGE')
    self.assertEqual(build.bad_revision, 1313185)


class ASANBuildTest(BisectTestCase):

  CommonDataXMLContent = '''<?xml version='1.0' encoding='UTF-8'?>
    <ListBucketResult xmlns='http://doc.s3.amazonaws.com/2006-03-01'>
      <Name>chromium-browser-asan</Name>
      <Prefix>mac-release/asan-mac-release</Prefix>
      <Marker></Marker>
      <NextMarker></NextMarker>
      <Delimiter>.zip</Delimiter>
      <IsTruncated>true</IsTruncated>
      <CommonPrefixes>
        <Prefix>mac-release/asan-mac-release-1313186.zip</Prefix>
      </CommonPrefixes>
      <CommonPrefixes>
        <Prefix>mac-release/asan-mac-release-1313195.zip</Prefix>
      </CommonPrefixes>
      <CommonPrefixes>
        <Prefix>mac-release/asan-mac-release-1313210.zip</Prefix>
      </CommonPrefixes>
    </ListBucketResult>
  '''

  @maybe_patch('urllib.request.urlopen',
               return_value=io.StringIO(CommonDataXMLContent))
  def test_get_rev_list(self, mock_urlopen):
    options = bisect_builds.ParseCommandLine([
        '--asan', '-a', 'mac', '-g', '1313161', '-b', '1313210',
        '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.ASANBuild)
    rev_list = build.get_rev_list()
    # print(mock_urlopen.call_args_list)
    mock_urlopen.assert_any_call(
        'http://commondatastorage.googleapis.com/chromium-browser-asan/'
        '?delimiter=.zip&prefix=mac-release/asan-mac-release'
        '&marker=mac-release/asan-mac-release-1313161.zip')
    self.assertEqual(mock_urlopen.call_count, 1)
    self.assertEqual(rev_list, [1313186, 1313195, 1313210])


class AndroidBuildTest(BisectTestCase):

  def setUp(self):
    # patch for devil_imports
    self.patchers = []
    flag_changer_patcher = maybe_patch('bisect-builds.flag_changer',
                                       create=True)
    self.patchers.append(flag_changer_patcher)
    self.mock_flag_changer = flag_changer_patcher.start()
    chrome_patcher = maybe_patch('bisect-builds.chrome', create=True)
    self.patchers.append(chrome_patcher)
    self.mock_chrome = chrome_patcher.start()
    version_codes_patcher = maybe_patch('bisect-builds.version_codes',
                                        create=True)
    self.patchers.append(version_codes_patcher)
    self.mock_version_codes = version_codes_patcher.start()
    self.mock_version_codes.LOLLIPOP = 21
    self.mock_version_codes.NOUGAT = 24
    self.mock_version_codes.PIE = 28
    self.mock_version_codes.Q = 29
    initial_android_device_patcher = patch(
        'bisect-builds.InitializeAndroidDevice')
    self.patchers.append(initial_android_device_patcher)
    self.mock_initial_android_device = initial_android_device_patcher.start()
    self.device = self.mock_initial_android_device.return_value
    self.set_sdk_level(bisect_builds.version_codes.Q)

  def set_sdk_level(self, level):
    self.device.build_version_sdk = level

  def tearDown(self):
    for patcher in self.patchers:
      patcher.stop()


class AndroidReleaseBuildTest(AndroidBuildTest):

  def setUp(self):
    super().setUp()
    self.set_sdk_level(bisect_builds.version_codes.PIE)

  @maybe_patch(
      'bisect-builds.GsutilList',
      return_value=[
          'gs://chrome-signed/android-B0urB0N/%s/arm_64/MonochromeStable.apk' %
          x for x in ['127.0.6533.76', '127.0.6533.78', '127.0.6533.79']
      ])
  def test_get_android_rev_list(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '--apk', 'chrome_stable', '-g',
        '127.0.6533.76', '-b', '127.0.6533.79', '--signed', '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    self.assertEqual(build.get_rev_list(),
                     ['127.0.6533.76', '127.0.6533.78', '127.0.6533.79'])
    mock_GsutilList.assert_any_call('gs://chrome-signed/android-B0urB0N')
    mock_GsutilList.assert_any_call(*[
        'gs://chrome-signed/android-B0urB0N/%s/arm_64/MonochromeStable.apk' % x
        for x in ['127.0.6533.76', '127.0.6533.78', '127.0.6533.79']
    ],
                                    ignore_fail=True)
    self.assertEqual(mock_GsutilList.call_count, 2)

  @patch('bisect-builds.InstallOnAndroid')
  def test_install_revision(self, mock_InstallOnAndroid):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '-g', '127.0.6533.76', '-b',
        '127.0.6533.79', '--apk', 'chrome'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    build._install_revision('chrome.apk', 'temp-dir')
    mock_InstallOnAndroid.assert_called_once_with(self.device, 'chrome.apk')

  @patch('bisect-builds.LaunchOnAndroid')
  def test_launch_revision(self, mock_LaunchOnAndroid):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '-g', '127.0.6533.76', '-b',
        '127.0.6533.79', '--apk', 'chrome'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    build._launch_revision('temp-dir', None)
    mock_LaunchOnAndroid.assert_called_once_with(self.device, 'chrome')

  @patch('bisect-builds.LaunchOnAndroid')
  def test_webview_launch_revision(self, mock_LaunchOnAndroid):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '-g', '127.0.6533.76', '-b',
        '127.0.6533.79', '--apk', 'system_webview'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    build._launch_revision('temp-dir', None)
    mock_LaunchOnAndroid.assert_called_once_with(self.device, 'system_webview')
    with self.assertRaises(bisect_builds.BisectException):
      build._launch_revision('temp-dir', None, ['args'])


class AndroidSnapshotBuildTest(AndroidBuildTest):

  def setUp(self):
    super().setUp()
    self.set_sdk_level(bisect_builds.version_codes.PIE)

  @patch('bisect-builds.InstallOnAndroid')
  @patch('bisect-builds.ArchiveBuild._install_revision',
         return_value={'chrome': 'chrome.apk'})
  def test_install_revision(self, mock_install_revision, mock_InstallOnAndroid):
    options = bisect_builds.ParseCommandLine([
        '-a', 'android-arm64', '-g', '1313161', '-b', '1313210', '--apk',
        'chrome'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidSnapshotBuild)
    build._install_revision('chrome.zip', 'temp-dir')
    mock_install_revision.assert_called_once_with('chrome.zip', 'temp-dir')
    mock_InstallOnAndroid.assert_called_once_with(self.device, 'chrome.apk')


class AndroidTrichromeReleaseBuildTest(AndroidBuildTest):

  def setUp(self):
    super().setUp()
    self.set_sdk_level(bisect_builds.version_codes.Q)

  @maybe_patch(
      'bisect-builds.GsutilList',
      side_effect=[[
          'gs://chrome-unsigned/android-B0urB0N/%s/' % x for x in [
              '129.0.6626.0', '129.0.6626.1', '129.0.6627.0', '129.0.6627.1',
              '129.0.6628.0', '129.0.6628.1'
          ]
      ],
                   [('gs://chrome-unsigned/android-B0urB0N/%s/'
                     'high-arm_64/TrichromeChromeGoogle6432Stable.apks') % x
                    for x in ['129.0.6626.0', '129.0.6627.0', '129.0.6628.0']]])
  def test_get_rev_list(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64-high', '--apk', 'chrome_stable', '-g',
        '129.0.6626.0', '-b', '129.0.6628.0', '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeReleaseBuild)
    self.assertEqual(build.get_rev_list(),
                     ['129.0.6626.0', '129.0.6627.0', '129.0.6628.0'])
    print(mock_GsutilList.call_args_list)
    mock_GsutilList.assert_any_call('gs://chrome-unsigned/android-B0urB0N')
    mock_GsutilList.assert_any_call(*[
        ('gs://chrome-unsigned/android-B0urB0N/%s/'
         'high-arm_64/TrichromeChromeGoogle6432Stable.apks') % x for x in [
             '129.0.6626.0', '129.0.6626.1', '129.0.6627.0', '129.0.6627.1',
             '129.0.6628.0'
         ]
    ],
                                    ignore_fail=True)
    self.assertEqual(mock_GsutilList.call_count, 2)

  def test_should_raise_exception_for_PIE(self):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64-high', '--apk', 'chrome_stable', '-g',
        '129.0.6626.0', '-b', '129.0.6667.0'
    ])
    self.set_sdk_level(bisect_builds.version_codes.PIE)
    with self.assertRaises(bisect_builds.BisectException):
      bisect_builds.create_archive_build(options)

  def test_get_download_url(self):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64-high', '--apk', 'chrome_stable', '-g',
        '129.0.6626.0', '-b', '129.0.6628.0'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeReleaseBuild)
    download_urls = build.get_download_url('129.0.6626.0')
    self.maxDiff = 1000
    self.assertDictEqual(
        download_urls, {
            'trichrome':
            ('gs://chrome-unsigned/android-B0urB0N/129.0.6626.0/high-arm_64/'
             'TrichromeChromeGoogle6432Stable.apks'),
            'trichrome_library':
            ('gs://chrome-unsigned/android-B0urB0N/129.0.6626.0/high-arm_64/'
             'TrichromeLibraryGoogle6432Stable.apk'),
        })

  @patch('bisect-builds.InstallOnAndroid')
  def test_install_revision(self, mock_InstallOnAndroid):
    downloads = {
        'trichrome': 'some-file.apks',
        'trichrome_library': 'file2.apk',
    }
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64-high', '--apk', 'chrome_stable', '-g',
        '129.0.6626.0', '-b', '129.0.6628.0'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeReleaseBuild)
    build._install_revision(downloads, 'tmp-dir')
    mock_InstallOnAndroid.assert_any_call(self.device, 'some-file.apks')
    mock_InstallOnAndroid.assert_any_call(self.device, 'file2.apk')


class AndroidTrichromeOfficialBuildTest(AndroidBuildTest):

  @maybe_patch('bisect-builds.GsutilList',
               return_value=[
                   'full-build-linux_%d.zip' % x
                   for x in [1334339, 1334342, 1334344, 1334345, 1334356]
               ])
  def test_get_rev_list(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'android-arm64-high', '--apk', 'chrome', '-g', '1334338',
        '-b', '1334380', '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeOfficialBuild)
    self.assertEqual(build.get_rev_list(),
                     [1334339, 1334342, 1334344, 1334345, 1334356])
    mock_GsutilList.assert_called_once_with(
        'gs://chrome-test-builds/official-by-commit/'
        'android_arm64_high_end-builder-perf/')

  def test_get_download_url(self):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'android-arm64-high', '--apk', 'chrome', '-g', '1334338',
        '-b', '1334380'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeOfficialBuild)
    self.assertEqual(
        build.get_download_url(1334338),
        'gs://chrome-test-builds/official-by-commit'
        '/android_arm64_high_end-builder-perf/full-build-linux_1334338.zip')

  @patch('glob.glob',
         side_effect=[[
             'temp-dir/full-build-linux/apks/TrichromeChromeGoogle6432.apks'
         ], ['temp-dir/full-build-linux/apks/TrichromeLibraryGoogle6432.apk']])
  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('bisect-builds.InstallOnAndroid')
  def test_install_revision(self, mock_InstallOnAndroid,
                            mock_UnzipFilenameToDir, mock_glob):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'android-arm64-high', '--apk', 'chrome', '-g', '1334338',
        '-b', '1334380'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeOfficialBuild)
    build._install_revision('download.zip', 'tmp-dir')
    mock_UnzipFilenameToDir.assert_called_once_with('download.zip', 'tmp-dir')
    mock_InstallOnAndroid.assert_any_call(
        self.device,
        'temp-dir/full-build-linux/apks/TrichromeLibraryGoogle6432.apk')
    mock_InstallOnAndroid.assert_any_call(
        self.device,
        'temp-dir/full-build-linux/apks/TrichromeChromeGoogle6432.apks')

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.InstallOnAndroid')
  @patch('bisect-builds.LaunchOnAndroid')
  def test_run_revision_with_real_zipfile(self, mock_LaunchOnAndroid,
                                          mock_InstallOnAndroid):
    options = bisect_builds.ParseCommandLine([
        '-o', '-a', 'android-arm64-high', '--apk', 'chrome', '-g', '1334338',
        '-b', '1334380'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidTrichromeOfficialBuild)
    download_job = build.get_download_job(1334339)
    zip_file = download_job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(zip_file, tempdir, [])
    print(mock_InstallOnAndroid.call_args_list)
    self.assertRegex(mock_InstallOnAndroid.mock_calls[0].args[1],
                     'full-build-linux/apks/TrichromeLibraryGoogle6432.apk$')
    self.assertRegex(
        mock_InstallOnAndroid.mock_calls[1].args[1],
        'full-build-linux/apks/TrichromeChromeGoogle6432.minimal.apks$')
    mock_LaunchOnAndroid.assert_called_once_with(self.device, 'chrome')


class LinuxReleaseBuildTest(BisectTestCase):

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_launch_revision_should_has_no_sandbox(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    options = bisect_builds.ParseCommandLine(
        ['-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.LinuxReleaseBuild)
    build._launch_revision('temp-dir', {'chrome': 'temp-dir/linux64/chrome'},
                           [])
    mock_Popen.assert_called_once_with(
        'temp-dir/linux64/chrome --user-data-dir=temp-dir/profile --no-sandbox',
        cwd=None,
        shell=True,
        bufsize=-1,
        stdout=ANY,
        stderr=ANY)


class IOSReleaseBuildTest(BisectTestCase):

  @maybe_patch(
      'bisect-builds.GsutilList',
      side_effect=[[
          'gs://chrome-unsigned/ios-G1N/127.0.6533.76/',
          'gs://chrome-unsigned/ios-G1N/127.0.6533.77/',
          'gs://chrome-unsigned/ios-G1N/127.0.6533.78/'
      ],
                   [
                       'gs://chrome-unsigned/ios-G1N'
                       '/127.0.6533.76/iphoneos17.5/ios/10863/canary.ipa',
                       'gs://chrome-unsigned/ios-G1N'
                       '/127.0.6533.77/iphoneos17.5/ios/10866/canary.ipa',
                       'gs://chrome-unsigned/ios-G1N'
                       '/127.0.6533.78/iphoneos17.5/ios/10868/canary.ipa'
                   ]])
  def test_list_rev(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios', '--ipa=canary.ipa', '--device-id', '321', '-g',
        '127.0.6533.74', '-b', '127.0.6533.78', '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSReleaseBuild)
    self.assertEqual(build.get_rev_list(),
                     ['127.0.6533.76', '127.0.6533.77', '127.0.6533.78'])
    mock_GsutilList.assert_any_call('gs://chrome-unsigned/ios-G1N')
    mock_GsutilList.assert_any_call(*[
        'gs://chrome-unsigned/ios-G1N/%s/*/ios/*/canary.ipa' % x
        for x in ['127.0.6533.76', '127.0.6533.77', '127.0.6533.78']
    ],
                                    ignore_fail=True)

  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('glob.glob', return_value=['Payload/canary.app/Info.plist'])
  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_install_revision(self, mock_Popen, mock_glob,
                            mock_UnzipFilenameToDir):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios', '--ipa=canary.ipa', '--device-id', '321', '-g',
        '127.0.6533.74', '-b', '127.0.6533.78'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSReleaseBuild)
    build._install_revision('canary.ipa', 'tempdir')
    mock_glob.assert_called_once_with('tempdir/Payload/*/Info.plist')
    mock_Popen.assert_has_calls([
        call([
            'xcrun', 'devicectl', 'device', 'install', 'app', '--device', '321',
            'canary.ipa'
        ],
             cwd=None,
             shell=False,
             bufsize=-1,
             stdout=-1,
             stderr=-1),
        call([
            'plutil', '-extract', 'CFBundleIdentifier', 'raw',
            'Payload/canary.app/Info.plist'
        ],
             cwd=None,
             shell=False,
             bufsize=-1,
             stdout=-1,
             stderr=-1)
    ],
                                any_order=True)

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_launch_revision(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios', '--ipa=canary.ipa', '--device-id', '321', '-g',
        '127.0.6533.74', '-b', '127.0.6533.78', '--', 'args1', 'args2'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSReleaseBuild)
    build._launch_revision('tempdir', 'com.google.chrome.ios', options.args)
    mock_Popen.assert_any_call([
        'xcrun', 'devicectl', 'device', 'process', 'launch', '--device', '321',
        'com.google.chrome.ios', 'args1', 'args2'
    ],
                               cwd=None,
                               shell=False,
                               bufsize=-1,
                               stdout=-1,
                               stderr=-1)

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, 'stdout', ''))
  def test_run_revision(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios', '--ipa=canary.ipa', '--device-id', '321', '-g',
        '127.0.6533.74', '-b', '127.0.6533.78'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSReleaseBuild)
    job = build.get_download_job('127.0.6533.76')
    ipa = job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(ipa, tempdir, options.args)
    mock_run.assert_has_calls([
        call([
            'xcrun', 'devicectl', 'device', 'install', 'app', '--device', '321',
            ANY
        ]),
        call(['plutil', '-extract', 'CFBundleIdentifier', 'raw', ANY]),
        call([
            'xcrun', 'devicectl', 'device', 'process', 'launch', '--device',
            '321', 'stdout'
        ])
    ])


class IOSSimulatorReleaseBuildTest(BisectTestCase):

  @maybe_patch(
      'bisect-builds.GsutilList',
      side_effect=[
          [
              'gs://bling-archive/128.0.6534.0/',
              'gs://bling-archive/128.0.6534.1/',
              'gs://bling-archive/128.0.6535.0/',
              'gs://bling-archive/128.0.6535.1/',
              'gs://bling-archive/128.0.6536.0/',
          ],
          [
              'gs://bling-archive/128.0.6534.0/20240612011643/Chromium.tar.gz',
              'gs://bling-archive/128.0.6536.0/20240613011356/Chromium.tar.gz',
          ]
      ])
  def test_list_rev(self, mock_GsutilList):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios-simulator', '--device-id', '321', '-g', '128.0.6534.0',
        '-b', '128.0.6536.0', '--no-local-cache'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSSimulatorReleaseBuild)
    self.assertEqual(build.get_rev_list(), ['128.0.6534.0', '128.0.6536.0'])
    mock_GsutilList.assert_any_call('gs://bling-archive')
    mock_GsutilList.assert_any_call(*[
        'gs://bling-archive/%s/*/Chromium.tar.gz' % x for x in [
            '128.0.6534.0', '128.0.6534.1', '128.0.6535.0', '128.0.6535.1',
            '128.0.6536.0'
        ]
    ],
                                    ignore_fail=True)

  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('glob.glob', return_value=['Info.plist'])
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, '', ''))
  def test_install_revision(self, mock_run, mock_glob, mock_UnzipFilenameToDir):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios-simulator', '--device-id', '321', '-g', '128.0.6534.0',
        '-b', '128.0.6539.0'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSSimulatorReleaseBuild)
    build._install_revision('Chromium.tar.gz', 'tempdir')
    mock_UnzipFilenameToDir.assert_called_once_with('Chromium.tar.gz',
                                                    'tempdir')
    self.assertEqual(mock_glob.call_count, 2)
    mock_run.assert_has_calls([
        call(['xcrun', 'simctl', 'install', '321', ANY]),
        call(['plutil', '-extract', 'CFBundleIdentifier', 'raw', 'Info.plist']),
    ])

  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, '', ''))
  def test_launch_revision(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios-simulator', '--device-id', '321', '-g', '128.0.6534.0',
        '-b', '128.0.6539.0'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSSimulatorReleaseBuild)
    build._launch_revision('tempdir', 'com.google.chrome.ios.dev',
                           ['args1', 'args2'])
    mock_run.assert_any_call([
        'xcrun', 'simctl', 'launch', '321', 'com.google.chrome.ios.dev',
        'args1', 'args2'
    ])

  @unittest.skipUnless('NO_MOCK_SERVER' in os.environ,
                       'The test only valid when NO_MOCK_SERVER')
  @patch('bisect-builds.ArchiveBuild._run', return_value=(0, 'stdout', ''))
  def test_run_revision(self, mock_run):
    options = bisect_builds.ParseCommandLine([
        '-r', '-a', 'ios-simulator', '--device-id', '321', '-g', '128.0.6534.0',
        '-b', '128.0.6539.0'
    ])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.IOSSimulatorReleaseBuild)
    job = build.get_download_job('128.0.6534.0')
    download = job.start().wait_for()
    with tempfile.TemporaryDirectory(prefix='bisect_tmp') as tempdir:
      build.run_revision(download, tempdir, options.args)
    mock_run.assert_has_calls([
        call(['xcrun', 'simctl', 'install', '321', ANY]),
        call(['plutil', '-extract', 'CFBundleIdentifier', 'raw', ANY]),
        call(['xcrun', 'simctl', 'launch', '321', 'stdout'])
    ])


class MaybeSwitchBuildTypeTest(BisectTestCase):

  def test_generate_new_command_without_cache(self):
    command_line = [
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88',
        '--no-local-cache'
    ]
    options = bisect_builds.ParseCommandLine(command_line)
    with patch('sys.argv', ['bisect-builds.py', *command_line]):
      new_cmd = bisect_builds.MaybeSwitchBuildType(
          options, bisect_builds.LooseVersion('127.0.6533.74'),
          bisect_builds.LooseVersion('127.0.6533.88'))
      self.assertEqual(new_cmd[1:], [
          '-o', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88',
          '--verify-range', '--no-local-cache'
      ])

  def test_android_signed_with_args(self):
    command_line = [
        '-r', '--archive=android-arm64-high', '--good=127.0.6533.74', '-b',
        '127.0.6533.88', '--apk=chrome', '--signed', '--no-local-cache', '--',
        'args1', '--args2'
    ]
    options = bisect_builds.ParseCommandLine(command_line)
    with patch('sys.argv', ['bisect-builds.py', *command_line]):
      new_cmd = bisect_builds.MaybeSwitchBuildType(options, '127.0.6533.74',
                                                   '127.0.6533.88')
      self.assertEqual(new_cmd[1:], [
          '-o', '-a', 'android-arm64-high', '-g', '127.0.6533.74', '-b',
          '127.0.6533.88', '--verify-range', '--apk=chrome', '--no-local-cache',
          '--', 'args1', '--args2'
      ])

  def test_no_official_build(self):
    command_line = [
        '-r', '-a', 'ios', '--ipa=canary.ipa', '--device-id', '321', '-g',
        '127.0.6533.74', '-b', '127.0.6533.78', '--no-local-cache'
    ]
    options = bisect_builds.ParseCommandLine(command_line)
    with patch('sys.argv', ['bisect-builds.py', *command_line]):
      new_cmd = bisect_builds.MaybeSwitchBuildType(options, '127.0.6533.74',
                                                   '127.0.6533.88')
      self.assertEqual(new_cmd, None)

  @patch('bisect-builds.ArchiveBuild.get_rev_list', return_value=list(range(3)))
  def test_generate_suggestion_with_cache(self, mock_get_rev_list):
    command_line = [
        '-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88',
        '--use-local-cache'
    ]
    options = bisect_builds.ParseCommandLine(command_line)
    with patch('sys.argv', ['bisect-builds.py', *command_line]):
      new_cmd = bisect_builds.MaybeSwitchBuildType(options, '127.0.6533.74',
                                                   '127.0.6533.88')
      self.assertEqual(new_cmd[1:], [
          '-o', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88',
          '--verify-range', '--use-local-cache'
      ])
      mock_get_rev_list.assert_called()


class MethodTest(BisectTestCase):

  @patch('sys.stderr', new_callable=io.StringIO)
  def test_ParseCommandLine(self, mock_stderr):
    opts = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '1', 'args1', 'args2 3', '-b', '2'])
    self.assertEqual(opts.build_type, 'snapshot')
    self.assertEqual(opts.args, ['args1', 'args2 3'])

    opts = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '1', 'args1', 'args2 3'])
    self.assertEqual(opts.args, ['args1', 'args2 3'])

    opts = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '1', '--', 'args1', 'args2 3', '-b', '2'])
    self.assertEqual(opts.args, ['args1', 'args2 3', '-b', '2'])

    with self.assertRaises(SystemExit):
      bisect_builds.ParseCommandLine(['-a', 'mac64', '-o', '-g', '1'])
      self.assertRegexpMatches(
          mock_stderr.getvalue(), r'To bisect for mac64, please choose from '
          r'release(-r), snapshot(-s)')

  @patch("urllib.request.urlopen",
         side_effect=[
             urllib.request.HTTPError('url', 404, 'Not Found', None, None),
             urllib.request.HTTPError('url', 404, 'Not Found', None, None),
             io.StringIO("NOT_A_JSON"),
             io.StringIO('{"chromium_main_branch_position": 123}'),
         ])
  def test_GetRevisionFromVersion(self, mock_urlopen):
    self.assertEqual(123,
                     bisect_builds.GetRevisionFromVersion('127.0.6533.134'))
    mock_urlopen.assert_has_calls([
        call('https://chromiumdash.appspot.com/fetch_version'
             '?version=127.0.6533.134'),
        call('https://chromiumdash.appspot.com/fetch_version'
             '?version=127.0.6533.0'),
    ])

  @patch("urllib.request.urlopen",
         side_effect=[
             io.StringIO('{"chromium_main_branch_position": null}'),
             io.StringIO('{"message": "DEP\\n"}'),
             io.StringIO('{"message": "Cr-Branched-From: '
                         'e5ce7dc4f7518237b3d9bb93cccca35d25216cbe-'
                         'refs/heads/master@{#857950}\\n"}'),
         ])
  def test_GetRevisionFromSourceTag(self, mock_urlopen):
    self.assertEqual(857950,
                     bisect_builds.GetRevisionFromVersion('127.0.6533.134'))


if __name__ == '__main__':
  unittest.main()

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
import unittest
from unittest.mock import ANY, Mock, MagicMock, mock_open, patch

bisect_builds = __import__('bisect-builds')

if 'NO_MOCK_SERVER' not in os.environ:
  maybe_patch = patch
else:
  # SetupEnvironment for gsutil to connect to real server.
  options, _ = bisect_builds.ParseCommandLine(['-a', 'linux64'])
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
      super().__init__(spec, *args, **kwargs, wraps=spec)

  maybe_patch = functools.partial(patch, spec=True, new_callable=WrappedMock)
  maybe_patch.object = functools.partial(patch.object,
                                         spec=True,
                                         new_callable=WrappedMock)


class BisectTestCase(unittest.TestCase):

  @classmethod
  def setUpClass(cls):
    if sys.version_info[:2] <= (3, 8):
      return
    # Patch the name pattern for pkgutil to accept "bisect-builds" as module
    # name.
    dotted_words = r'(?!\d)([\w-]+)(\.(?!\d)(\w+))*'
    name_pattern = re.compile(
        f'^(?P<pkg>{dotted_words})'
        f'(?P<cln>:(?P<obj>{dotted_words})?)?$', re.UNICODE)
    cls.name_pattern_patcher = patch('pkgutil._NAME_PATTERN', name_pattern)
    cls.name_pattern_patcher.start()

  @classmethod
  def tearDownClass(cls):
    if sys.version_info[:2] <= (3, 8):
      return
    cls.name_pattern_patcher.stop()


class BisectTest(BisectTestCase):

  max_rev = 10000

  def bisect(self, good_rev, bad_rev, evaluate, num_runs=1):
    options, args = bisect_builds.ParseCommandLine([
        '-a', 'linux64', '-g', good_rev, '-b', bad_rev, '--times',
        str(num_runs)
    ])
    archive_build = bisect_builds.create_archive_build(options)
    (minrev, maxrev) = bisect_builds.Bisect(archive_build=archive_build,
                                            evaluate=evaluate,
                                            try_args=args)
    return (minrev, maxrev)

  @patch('bisect-builds.DownloadJob.fetch')
  @patch('bisect-builds.ArchiveBuild.run_revision', return_value=(0, '', ''))
  @patch('bisect-builds.SnapshotBuild._get_rev_list',
         return_value=range(max_rev))
  def testBisectConsistentAnswer(self, mock_get_rev_list, mock_run_revision,
                                 mock_fetch):
    self.assertEqual(self.bisect(1000, 100, lambda *args: 'g'), (100, 101))
    self.assertEqual(self.bisect(100, 1000, lambda *args: 'b'), (100, 101))
    self.assertEqual(self.bisect(2000, 200, lambda *args: 'b'), (1999, 2000))
    self.assertEqual(self.bisect(200, 2000, lambda *args: 'g'), (1999, 2000))


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
  @patch('os.close')
  @patch('os.unlink')
  def test_should_del(self, mock_unlink, mock_close, mock_mkstemp):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    del fetch
    mock_unlink.assert_called_with('some-file.zip')
    mock_close.assert_called_once()
    mock_mkstemp.assert_called_once()

  @patch('urllib.request.urlretrieve')
  def test_stop_wait_for_should_be_able_to_reenter(self, mock_urlretrieve):
    fetch = bisect_builds.DownloadJob('http://some-file.zip', 123)
    fetch.start()
    fetch.wait_for()
    fetch.wait_for()
    fetch.stop()
    fetch.stop()
    fetch.wait_for()
    fetch.stop()


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

  def create_build(self, args=None):
    if args is None:
      args = ['-a', 'linux64', '-g', '0', '-b', '9']
    options, args = bisect_builds.ParseCommandLine(args)
    return bisect_builds.ArchiveBuild(options)

  def test_cache_should_not_work_if_not_enabled(self):
    build = self.create_build()
    self.assertFalse(build.use_local_cache)
    with patch('builtins.open') as m:
      self.assertEqual(build.get_rev_list(), [str(x) for x in range(10)])
      bisect_builds.ArchiveBuild._get_rev_list.assert_called_once()
      m.assert_not_called()

  def test_cache_should_save_and_load(self):
    build = self.create_build(
        ['-a', 'linux64', '-g', '0', '-b', '9', '--use-local-cache'])
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
    build = self.create_build(
        ['-a', 'linux64', '-g', '0', '-b', '9', '--use-local-cache'])
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
    build = self.create_build()
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
    build = self.create_build()
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
    build.run_revision('', [])
    mock_launch_revision.assert_called_once()

  @patch('bisect-builds.ArchiveBuild._install_revision')
  @patch('bisect-builds.ArchiveBuild._launch_revision',
         return_value=(0, '', ''))
  def test_run_revision_should_do_all_runs(self, mock_launch_revision,
                                           mock_install_revision):
    build = self.create_build(
        ['-a', 'linux64', '-g', '0', '-b', '9', '--time', '10'])
    build.run_revision('', [])
    self.assertEqual(mock_launch_revision.call_count, 10)

  @patch('bisect-builds.UnzipFilenameToDir')
  @patch('glob.glob', return_value=['temp-dir/linux64/chrome'])
  @patch('os.path.abspath', return_value='/tmp/temp-dir/linux64/chrome')
  def test_install_revision_should_unzip_and_search_executable(
      self, mock_abspath, mock_glob, mock_UnzipFilenameToDir):
    build = self.create_build()
    self.assertEqual(build._install_revision('some-file.zip', 'temp-dir'),
                     '/tmp/temp-dir/linux64/chrome')
    mock_UnzipFilenameToDir.assert_called_once_with('some-file.zip', 'temp-dir')
    mock_glob.assert_called_once_with('temp-dir/*/chrome')
    mock_abspath.assert_called_once_with('temp-dir/linux64/chrome')

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_launch_revision_should_run_command(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    build = self.create_build()
    build._launch_revision('temp-dir', 'temp-dir/linux64/chrome', [])
    mock_Popen.assert_called_once_with(
        ['temp-dir/linux64/chrome', '--user-data-dir=profile'],
        cwd='temp-dir',
        bufsize=-1,
        stdout=ANY,
        stderr=ANY)


class ReleaseBuildTest(BisectTestCase):

  def test_should_look_up_path_context(self):
    options, args = bisect_builds.ParseCommandLine(
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
    options, args = bisect_builds.ParseCommandLine(
        ['-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.76'])
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
    options, args = bisect_builds.ParseCommandLine([
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


class ArchiveBuildWithCommitPositionTest(BisectTestCase):

  def setUp(self):
    patch.multiple(bisect_builds.ArchiveBuildWithCommitPosition,
                   __abstractmethods__=set(),
                   build_type='release').start()

  @maybe_patch('bisect-builds.GetRevisionFromVersion', return_value=1313161)
  @maybe_patch('bisect-builds.GetChromiumRevision', return_value=999999999)
  def test_should_convert_revision_as_commit_position(
      self, mock_GetChromiumRevision, mock_GetRevisionFromVersion):
    options, args = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '127.0.6533.74'])
    build = bisect_builds.ArchiveBuildWithCommitPosition(options)
    self.assertEqual(build.good_revision, 1313161)
    self.assertEqual(build.bad_revision, 999999999)
    mock_GetRevisionFromVersion.assert_called_once_with('127.0.6533.74')
    mock_GetChromiumRevision.assert_called()


class OfficialBuildTest(BisectTestCase):

  def test_should_lookup_path_context(self):
    options, args = bisect_builds.ParseCommandLine(
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
    options, args = bisect_builds.ParseCommandLine(
        ['-o', '-a', 'linux64', '-g', '1313161', '-b', '1313163'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.OfficialBuild)
    self.assertEqual(build.get_rev_list(), list(range(1313161, 1313164)))
    mock_GsutilList.assert_called_once_with(
        'gs://chrome-test-builds/official-by-commit/linux-builder-perf/')


class SnapshotBuildTest(BisectTestCase):

  def test_should_lookup_path_context(self):
    options, args = bisect_builds.ParseCommandLine(
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
    options, args = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '1313161', '-b', '1313185'])
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
    options, args = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '0', '-b', '9'])
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
    options, args = bisect_builds.ParseCommandLine(
        ['-a', 'linux64', '-g', '3', '-b', '11'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.SnapshotBuild)
    rev_list = build._get_rev_list(0, 11)
    self.assertEqual(sorted(rev_list), list(range(1, 11)))
    mock_fetch_and_parse.assert_called_once_with(
        'http://commondatastorage.googleapis.com/chromium-browser-snapshots/'
        '?delimiter=/&prefix=Linux_x64/')


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
    options, args = bisect_builds.ParseCommandLine(
        ['--asan', '-a', 'mac', '-g', '1313161', '-b', '1313210'])
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

  def tearDown(self):
    for patcher in self.patchers:
      patcher.stop()

  @maybe_patch(
      'bisect-builds.GsutilList',
      return_value=[
          'gs://chrome-signed/android-B0urB0N/%s/arm_64/MonochromeStable.apk' %
          x for x in ['127.0.6533.76', '127.0.6533.78', '127.0.6533.79']
      ])
  @maybe_patch('bisect-builds._GetMappingFromAndroidApk',
               return_value=bisect_builds.MONOCHROME_APK_FILENAMES)
  @patch('bisect-builds.InitializeAndroidDevice')
  def test_get_android_rev_list(self, mock_InitializeAndroidDevice,
                                mock_GetMapping, mock_GsutilList):
    options, args = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '--apk', 'chrome_stable', '-g',
        '127.0.6533.76', '-b', '127.0.6533.79', '--signed'
    ])
    # version_codes.S
    mock_InitializeAndroidDevice.return_value.build_version_sdk = 31
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
  @patch('bisect-builds.InitializeAndroidDevice')
  def test_install_revision(self, mock_InitializeAndroidDevice,
                            mock_InstallOnAndroid):
    options, args = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '-g', '1313161', '-b', '1313210', '--apk',
        'chrome'
    ])
    mock_InitializeAndroidDevice.return_value.build_version_sdk = 31
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    build._install_revision('chrome.apk', 'temp-dir')
    mock_InstallOnAndroid.assert_called_once_with(
        mock_InitializeAndroidDevice.return_value, 'chrome.apk')

  @patch('bisect-builds.LaunchOnAndroid')
  @patch('bisect-builds.InitializeAndroidDevice')
  def test_launch_revision(self, mock_InitializeAndroidDevice,
                           mock_LaunchOnAndroid):
    options, args = bisect_builds.ParseCommandLine([
        '-r', '-a', 'android-arm64', '-g', '1313161', '-b', '1313210', '--apk',
        'chrome'
    ])
    mock_InitializeAndroidDevice.return_value.build_version_sdk = 31
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidReleaseBuild)
    build._launch_revision('temp-dir', None)
    mock_LaunchOnAndroid.assert_called_once_with(
        mock_InitializeAndroidDevice.return_value, 'chrome')

  @patch('bisect-builds.InstallOnAndroid')
  @patch('bisect-builds.InitializeAndroidDevice')
  @patch('bisect-builds.ArchiveBuild._install_revision',
         return_value='chrome.apk')
  def test_install_revision(self, mock_install_revision,
                            mock_InitializeAndroidDevice,
                            mock_InstallOnAndroid):
    options, args = bisect_builds.ParseCommandLine([
        '-a', 'android-arm64', '-g', '1313161', '-b', '1313210', '--apk',
        'chrome'
    ])
    mock_InitializeAndroidDevice.return_value.build_version_sdk = 31
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.AndroidSnapshotBuild)
    build._install_revision('chrome.zip', 'temp-dir')
    mock_install_revision.assert_called_once_with('chrome.zip', 'temp-dir')
    mock_InstallOnAndroid.assert_called_once_with(
        mock_InitializeAndroidDevice.return_value, 'chrome.apk')


class LinuxReleaseBuildTest(BisectTestCase):

  @patch('subprocess.Popen', spec=subprocess.Popen)
  def test_launch_revision_should_has_no_sandbox(self, mock_Popen):
    mock_Popen.return_value.communicate.return_value = ('', '')
    mock_Popen.return_value.returncode = 0
    options, args = bisect_builds.ParseCommandLine(
        ['-r', '-a', 'linux64', '-g', '127.0.6533.74', '-b', '127.0.6533.88'])
    build = bisect_builds.create_archive_build(options)
    self.assertIsInstance(build, bisect_builds.LinuxReleaseBuild)
    build._launch_revision('temp-dir', 'temp-dir/linux64/chrome', [])
    mock_Popen.assert_called_once_with(
        ['temp-dir/linux64/chrome', '--user-data-dir=profile', '--no-sandbox'],
        cwd='temp-dir',
        bufsize=-1,
        stdout=ANY,
        stderr=ANY)


if __name__ == '__main__':
  unittest.main()

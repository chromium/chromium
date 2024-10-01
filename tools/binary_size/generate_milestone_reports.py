#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate report files to view and/or compare (diff) milestones.

Size files are located in a Google Cloud Storage bucket for various Chrome
versions. This script generates various HTML report files to view a single
milesone, or to compare two milestones with the same CPU and APK.

Desired CPUs, APKs, and milestone versions are set in constants below. If
specified by the --skip-existing flag, the script checks what HTML report files
have already been uploaded to the GCS bucket, then works on generating the
remaining desired files.

Reports can be uploaded automatically with the --sync flag. Otherwise, they can
be uploaded at a later point.
"""

import argparse
import collections
import contextlib
import errno
import itertools
import json
import logging
import multiprocessing
import os
import re
import shutil
import sys
import subprocess
import tempfile

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
_GSUTIL = os.path.join(_DIR_SOURCE_ROOT, 'third_party', 'depot_tools',
                       'gsutil.py')

_PUSH_URL = 'gs://chrome-supersize/milestones/'

_DESIRED_CPUS = ['arm', 'arm_64', 'high-arm_64']
_DESIRED_APKS = ['Monochrome.apk', 'AndroidWebview.apk', 'TrichromeGoogle']

# Versions are manually gathered from
# https://omahaproxy.appspot.com/history?os=android&channel=stable
_DESIRED_VERSIONS = [
    '60.0.3112.116',
    '61.0.3163.98',
    '62.0.3202.84',
    '63.0.3239.111',
    '64.0.3282.137',
    '65.0.3325.85',
    '66.0.3359.158',
    '67.0.3396.87',
    '68.0.3440.85',
    '69.0.3497.91',
    '70.0.3538.64',
    '71.0.3578.99',
    '72.0.3626.105',
    '73.0.3683.75',
    '74.0.3729.112',
    '75.0.3770.143',
    '76.0.3809.132',
    '77.0.3865.115',
    '78.0.3904.62',
    '79.0.3945.136',
    '80.0.3987.99',
    '81.0.4044.138',
    '83.0.4103.60',
    '84.0.4147.89',
    '85.0.4183.81',
    '86.0.4240.198',
    '87.0.4280.66',
    '88.0.4324.93',
    '89.0.4389.105',
    '90.0.4430.82',
    '91.0.4472.120',
    '92.0.4515.70',
    '93.0.4577.37',
    '94.0.4606.6',
    '94.0.4606.85',
    '95.0.4638.7',
    '96.0.4664.6',
    '97.0.4692.9',
    '98.0.4758.8',
    '99.0.4844.7',
    '100.0.4896.12',
    '101.0.4951.20',
    '102.0.5005.37',
    '103.0.5060.9',
    '104.0.5112.9',
    '105.0.5195.7',
    '106.0.5249.7',
    '107.0.5304.14',
    '108.0.5359.12',
    '109.0.5414.8',
    '110.0.5481.29',
    '111.0.5563.31',
    '112.0.5615.7',
    '113.0.5672.10',
    '114.0.5735.4',
    '115.0.5790.5',
    '116.0.5845.20',
    '117.0.5938.5',
    '118.0.5993.5',
    '119.0.6045.7',
    '120.0.6099.18',
    '121.0.6167.7',
    '122.0.6261.8',
    '123.0.6312.54',
    '124.0.6367.47',
    '125.0.6422.3',
    '126.0.6478.16',
    '127.0.6533.27',
    '128.0.6613.20',
    '129.0.6668.32',
    '130.0.6723.20',
]


def _VersionMajor(version):
  return tuple(int(x) for x in version.split('.'))[0]


def _IsBundle(apk, version):
  version = _VersionMajor(version)
  if apk == 'Monochrome.apk' and version >= 73:
    return True
  if apk == 'AndroidWebview.apk' and version >= 89:
    return True
  return False


def _EnumerateReports():
  for cpu, apk in itertools.product(_DESIRED_CPUS, _DESIRED_APKS):
    versions = _DESIRED_VERSIONS
    if cpu == 'high-arm_64':
      if apk != 'TrichromeGoogle':
        continue
      versions = [v for v in versions if _VersionMajor(v) >= 126]
    elif apk == 'AndroidWebview.apk':
      # Webview .size files do not exist before M71.
      versions = [v for v in versions if _VersionMajor(v) >= 71]
    elif apk == 'TrichromeGoogle':
      versions = [v for v in versions if _VersionMajor(v) >= 88]

    # Switched to high-end only.
    if cpu == 'arm_64':
      versions = [v for v in versions if _VersionMajor(v) < 127]

    for version in versions:
      yield Report(cpu, apk, version)


class Report(collections.namedtuple('Report', 'cpu,apk,version')):

  def GetSizeFileSubpath(self, local):
    # TrichromeGoogle at older milestones lived in a subdir.
    if not local and self.apk == 'TrichromeGoogle' and _VersionMajor(
        self.version) < 91:
      template = '{version}/{cpu}/for-signing-only/{apk}.size'
    elif self.cpu == 'high-arm_64':
      template = '{version}/{cpu}/{apk}6432.size'
    else:
      template = '{version}/{cpu}/{apk}.size'

    ret = template.format(**self._asdict())

    if not local and _IsBundle(self.apk, self.version):
      ret = ret.replace('.apk', '.minimal.apks')

    return ret


def _MakeDirectory(path):
  # Function is safe even from racing fork()ed processes.
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def _Shard(func, arg_tuples):
  pool = multiprocessing.Pool()
  try:
    for x in pool.imap_unordered(func, arg_tuples):
      yield x
  finally:
    pool.close()


def _DownloadOneSizeFile(arg_tuples):
  subpath, temp_dir, base_url = arg_tuples
  src = '{}/{}'.format(base_url, subpath)
  dest = os.path.join(temp_dir, subpath)
  _MakeDirectory(os.path.dirname(dest))
  subprocess.check_call([_GSUTIL, '-q', 'cp', src, dest])


@contextlib.contextmanager
def _DownloadSizeFiles(base_url, reports):
  temp_dir = tempfile.mkdtemp()
  try:
    subpaths = set(x.GetSizeFileSubpath(local=False) for x in reports)
    arg_tuples = ((p, temp_dir, base_url) for p in subpaths)
    for _ in _Shard(_DownloadOneSizeFile, arg_tuples):
      pass
    yield temp_dir
  finally:
    shutil.rmtree(temp_dir)


def _WriteMilestonesJson(path):
  with open(path, 'w') as out_file:
    # TODO(agrieve): Record the full list of reports rather than three arrays
    #    so that the UI can prevent selecting non-existent entries.
    pushed_reports_obj = {
        'pushed': {
            'apk': _DESIRED_APKS,
            'cpu': _DESIRED_CPUS,
            'version': _DESIRED_VERSIONS,
        },
    }
    json.dump(pushed_reports_obj, out_file, sort_keys=True, indent=2)


def _BuildOneReport(report, output_directory, size_file_directory):
  # Newer Monochrome builds are minimal builds, with names like
  # "Monochrome.minimal.apks.size". Standardize to "Monochrome.apk.size".
  local_size_path = os.path.join(output_directory,
                                 report.GetSizeFileSubpath(local=True))
  _MakeDirectory(os.path.dirname(local_size_path))

  size_file = os.path.join(size_file_directory,
                           report.GetSizeFileSubpath(local=False))
  shutil.copyfile(size_file, local_size_path)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--size-file-bucket',
      required=True,
      help='GCS bucket to find size files in. (e.g. "gs://bucket/subdir")')
  parser.add_argument(
      '--sync',
      action='store_true',
      help='Sync data files to GCS (otherwise just prints out command to run).')
  parser.add_argument('--wait',
                      action='store_true',
                      help='Allow user to examine staged content before exit.')
  args = parser.parse_args()

  size_file_bucket = args.size_file_bucket.rstrip('/')
  if not size_file_bucket.startswith('gs://'):
    parser.error('Size file bucket must start with gs://')

  reports_to_make = set(_EnumerateReports())

  logging.warning('Downloading %d size files.', len(reports_to_make))
  with _DownloadSizeFiles(args.size_file_bucket, reports_to_make) as sizes_dir:
    staging_dir = os.path.join(sizes_dir, 'staging')
    _MakeDirectory(staging_dir)
    if not args.sync:
      logging.warning('Staging dir: %s', staging_dir)

    for r in reports_to_make:
      _BuildOneReport(r, staging_dir, sizes_dir)

    _WriteMilestonesJson(os.path.join(staging_dir, 'milestones.json'))

    if args.sync:
      subprocess.check_call(
          [_GSUTIL, '-m', 'rsync', '-r', staging_dir, _PUSH_URL])
      milestones_json = _PUSH_URL + 'milestones.json'
      # The main index.html page has no authentication code, so make .json file
      # world-readable.
      subprocess.check_call(
          [_GSUTIL, 'acl', 'set', '-a', 'public-read', milestones_json])
      subprocess.check_call(
          [_GSUTIL, 'setmeta', '-h', 'Cache-Control:no-cache', milestones_json])
    else:
      logging.warning('Finished dry run. Run with --sync to upload.')
    if args.wait:
      input('Press <enter> to delete staging dir %s, and finish.' % staging_dir)


if __name__ == '__main__':
  main()

#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
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

Size files are fetched by streaming them from the source bucket, then the
html_report module handles creating a report file to diff two size files.
Reports are saved to a local directory, and once all reports are created they
can be uploaded to the destination bucket.

Reports can be uploaded automatically with the --sync flag. Otherwise, they can
be uploaded at a later point.
"""

from __future__ import print_function

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

_DESIRED_CPUS = ['arm', 'arm_64']
# Measure Chrome.apk since it's not a bundle.
_DESIRED_APKS = ['Monochrome.apk', 'Chrome.apk', 'AndroidWebview.apk']
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
    '79.0.3945.2',  # Canary
]


def _VersionTuple(version):
  return tuple(int(x) for x in version.split('.'))


def _IsBundle(apk, version):
  return apk == 'Monochrome.apk' and _VersionTuple(version) >= (73,)


def _EnumerateReports():
  for cpu, apk in itertools.product(_DESIRED_CPUS, _DESIRED_APKS):
    # KitKat doesn't support arm64.
    if cpu == 'arm_64' and apk == 'Chrome.apk':
      continue
    versions = _DESIRED_VERSIONS
    # Webview .size files do not exist before M71.
    if apk == 'AndroidWebview.apk':
      versions = [v for v in versions if _VersionTuple(v) >= (71,)]

    for after_version in versions:
      yield Report(cpu, apk, None, after_version)
    for i, before_version in enumerate(versions):
      for after_version in versions[i + 1:]:
        yield Report(cpu, apk, before_version, after_version)


def _TemplateToRegex(template):
  # Transform '{cpu}/{apk}/... -> (?P<cpu>[^/]+)/(?P<apk>[^/]+)/...
  pattern = re.sub(r'{(.*?)}', r'(?P<\1>[^/]+)', template)
  return re.compile(pattern)


class Report(
    collections.namedtuple('Report', 'cpu,apk,before_version,after_version')):

  _NDJSON_TEMPLATE_VIEW = '{cpu}/{apk}/report_{after_version}.ndjson'
  _NDJSON_TEMPLATE_COMPARE = (
      '{cpu}/{apk}/report_{before_version}_{after_version}.ndjson')
  _PUSH_URL_REGEX_VIEW = _TemplateToRegex(_PUSH_URL + _NDJSON_TEMPLATE_VIEW)
  _PUSH_URL_REGEX_COMPARE = _TemplateToRegex(_PUSH_URL +
                                             _NDJSON_TEMPLATE_COMPARE)

  @classmethod
  def FromUrl(cls, url):
    # Perform this match first since it's more restrictive.
    match = cls._PUSH_URL_REGEX_COMPARE.match(url)
    if match:
      return cls(**match.groupdict())
    match = cls._PUSH_URL_REGEX_VIEW.match(url)
    if match:
      return cls(before_version=None, **match.groupdict())
    return None

  def _CreateSizeSubpath(self, version):
    ret = '{version}/{cpu}/{apk}.size'.format(version=version, **self._asdict())
    if _IsBundle(self.apk, version):
      ret = ret.replace('.apk', '.minimal.apks')
    return ret

  @property
  def before_size_file_subpath(self):
    if self.before_version:
      return self._CreateSizeSubpath(self.before_version)
    return None

  @property
  def after_size_file_subpath(self):
    return self._CreateSizeSubpath(self.after_version)

  @property
  def ndjson_subpath(self):
    if self.before_version:
      return self._NDJSON_TEMPLATE_COMPARE.format(**self._asdict())
    return self._NDJSON_TEMPLATE_VIEW.format(**self._asdict())


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
    subpaths = set(x.after_size_file_subpath for x in reports)
    subpaths.update(x.before_size_file_subpath
                    for x in reports
                    if x.before_size_file_subpath)
    logging.warning('Downloading %d .size files', len(subpaths))
    arg_tuples = ((p, temp_dir, base_url) for p in subpaths)
    for _ in _Shard(_DownloadOneSizeFile, arg_tuples):
      pass
    yield temp_dir
  finally:
    shutil.rmtree(temp_dir)


def _FetchExistingMilestoneReports():
  milestones = subprocess.check_output([_GSUTIL, 'ls', '-R', _PUSH_URL + '*'])
  for path in milestones.splitlines()[1:]:
    report = Report.FromUrl(path)
    if report:
      yield report


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


def _BuildOneReport(arg_tuples):
  report, output_directory, size_file_directory = arg_tuples
  ndjson_path = os.path.join(output_directory, report.ndjson_subpath)

  _MakeDirectory(os.path.dirname(ndjson_path))
  script = os.path.join(os.path.dirname(__file__), 'supersize')
  after_size_file = os.path.join(size_file_directory,
                                 report.after_size_file_subpath)
  args = [script, 'html_report', after_size_file, ndjson_path]

  if report.before_version:
    before_size_file = os.path.join(size_file_directory,
                                    report.before_size_file_subpath)
    args += ['--diff-with', before_size_file]

  subprocess.check_output(args, stderr=subprocess.STDOUT)


def _CreateReportObjects(skip_existing):
  desired_reports = set(_EnumerateReports())
  logging.warning('Querying storage bucket for existing reports.')
  existing_reports = set(_FetchExistingMilestoneReports())
  missing_reports = desired_reports - existing_reports
  stale_reports = existing_reports - desired_reports
  if stale_reports:
    # Stale reports happen when we remove a version
    # (e.g. update a beta to a stable).
    # It's probably best to leave them in case people have linked to them.
    logging.warning('Number of stale reports: %d', len(stale_reports))
  if skip_existing:
    return sorted(missing_reports)
  return sorted(desired_reports)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      'directory', help='Directory to save report files to (must not exist).')
  parser.add_argument(
      '--size-file-bucket',
      required=True,
      help='GCS bucket to find size files in. (e.g. "gs://bucket/subdir")')
  parser.add_argument(
      '--sync',
      action='store_true',
      help='Sync data files to GCS (otherwise just prints out command to run).')
  parser.add_argument(
      '--skip-existing', action='store_true', help='Skip existing reports.')

  args = parser.parse_args()
  # Anything lower than WARNING gives screens full of supersize logs.
  logging.basicConfig(
      level=logging.WARNING,
      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  size_file_bucket = args.size_file_bucket.rstrip('/')
  if not size_file_bucket.startswith('gs://'):
    parser.error('Size file bucket must start with gs://')

  _MakeDirectory(args.directory)
  if os.listdir(args.directory):
    parser.error('Directory must be empty')

  reports_to_make = _CreateReportObjects(args.skip_existing)
  if not reports_to_make:
    logging.warning('No reports need to be created (due to --skip-existing).')
    return

  with _DownloadSizeFiles(args.size_file_bucket, reports_to_make) as sizes_dir:
    logging.warning('Generating %d reports.', len(reports_to_make))

    arg_tuples = ((r, args.directory, sizes_dir) for r in reports_to_make)
    for i, _ in enumerate(_Shard(_BuildOneReport, arg_tuples)):
      sys.stdout.write('\rGenerated {} of {}'.format(i + 1,
                                                     len(reports_to_make)))
      sys.stdout.flush()
    sys.stdout.write('\n')

  _WriteMilestonesJson(os.path.join(args.directory, 'milestones.json'))

  logging.warning('Reports saved to %s', args.directory)
  cmd = [
      _GSUTIL,
      '-m',
      'rsync',
      '-J',
      '-a',
      'public-read',
      '-r',
      args.directory,
      _PUSH_URL,
  ]

  if args.sync:
    subprocess.check_call(cmd)
  else:
    print()
    print('Sync files by running:')
    print('   ', ' '.join(cmd))


if __name__ == '__main__':
  main()

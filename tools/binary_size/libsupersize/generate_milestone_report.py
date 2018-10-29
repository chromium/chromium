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

import argparse
import codecs
import collections
import cStringIO
import errno
import itertools
import json
import logging
import multiprocessing
import os
import re
import subprocess

import archive
import diff
import html_report


PUSH_URL = 'gs://chrome-supersize/milestones/'
REPORT_URL_TEMPLATE_VIEW = '{cpu}/{apk}/report_{version2}.ndjson'
REPORT_URL_TEMPLATE_COMP = '{cpu}/{apk}/report_{version1}_{version2}.ndjson'

DESIRED_CPUS = ['arm', 'arm_64']
DESIRED_APKS = ['Monochrome.apk', 'ChromeModern.apk', 'AndroidWebview.apk']
# Versions are manually gathered from
# https://omahaproxy.appspot.com/history?os=android&channel=stable
DESIRED_VERSIONS = [
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
  '70.0.3538.17',  # Beta
  '71.0.3574.0',  # Dev
]


def _GetDesiredVersions(apk):
  if apk != 'AndroidWebview.apk':
    return DESIRED_VERSIONS
  # Webview .size files do not exist before M71.
  return [v for v in DESIRED_VERSIONS if int(v.split('.')[0]) >= 71]


class Report(collections.namedtuple(
  'Report', ['cpu', 'apk', 'version1', 'version2'])):
  PUSH_URL_REGEX_VIEW = re.compile((PUSH_URL + REPORT_URL_TEMPLATE_VIEW).format(
    cpu=r'(?P<cpu>[\w.]+)',
    apk=r'(?P<apk>[\w.]+)',
    version2=r'(?P<version2>[\w.]+)'
  ))

  PUSH_URL_REGEX_COMP = re.compile((PUSH_URL + REPORT_URL_TEMPLATE_COMP).format(
    cpu=r'(?P<cpu>[\w.]+)',
    apk=r'(?P<apk>[\w.]+)',
    version1=r'(?P<version1>[\w.]+)',
    version2=r'(?P<version2>[\w.]+)'
  ))

  @classmethod
  def FromUrl(cls, url):
    # Perform this match first since it's more restrictive.
    match = cls.PUSH_URL_REGEX_COMP.match(url)
    if match:
      return cls(
        match.group('cpu'),
        match.group('apk'),
        match.group('version1'),
        match.group('version2'),
      )
    match = cls.PUSH_URL_REGEX_VIEW.match(url)
    if match:
      return cls(
        match.group('cpu'),
        match.group('apk'),
        None,
        match.group('version2'),
      )
    return None


def _FetchExistingMilestoneReports():
  milestones = subprocess.check_output(['gsutil.py', 'ls', '-R',
                                         PUSH_URL + '*'])
  for path in milestones.splitlines()[1:]:
    report = Report.FromUrl(path)
    if report:
      yield report


def _SizeInfoFromGsPath(path):
  size_contents = subprocess.check_output(['gsutil.py', 'cat', path])
  file_obj = cStringIO.StringIO(size_contents)
  ret = archive.LoadAndPostProcessSizeInfo(path, file_obj=file_obj)
  file_obj.close()
  return ret


def _PossibleReportFiles():
  cpu_and_apk_combos = list(itertools.product(DESIRED_CPUS, DESIRED_APKS))
  for cpu, apk in cpu_and_apk_combos:
    apk_versions = _GetDesiredVersions(apk)
    for version2 in apk_versions:
      yield Report(cpu, apk, None, version2)
    for i, version1 in enumerate(apk_versions):
      for version2 in apk_versions[i + 1:]:
        yield Report(cpu, apk, version1, version2)


def _SetPushedReports(directory):
  outpath = os.path.join(directory, 'milestones.json')
  with codecs.open(outpath, 'w', encoding='ascii') as out_file:
    pushed_reports_obj = {
      'pushed': {
        'cpu': DESIRED_CPUS,
        'apk': DESIRED_APKS,
        'version': DESIRED_VERSIONS,
      },
    }
    json.dump(pushed_reports_obj, out_file)
    out_file.write('\n')


def _GetReportPaths(directory, template, report):
  report_dict = report._asdict()
  after_size_path = template.format(version=report.version2, **report_dict)
  if report.version1 is None:
    before_size_path = None
    out_rel = os.path.join(directory,
                           REPORT_URL_TEMPLATE_VIEW.format(**report_dict))
  else:
    before_size_path = template.format(version=report.version1,
                                                   **report_dict)
    out_rel = os.path.join(directory,
                           REPORT_URL_TEMPLATE_COMP.format(**report_dict))
  out_abs = os.path.abspath(out_rel)
  return (before_size_path, after_size_path, out_abs)


def _BuildReport(paths):
  before_size_path, after_size_path, outpath = paths
  try:
    os.makedirs(os.path.dirname(outpath))
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise

  size_info = _SizeInfoFromGsPath(after_size_path)
  if before_size_path:
    size_info = diff.Diff(_SizeInfoFromGsPath(before_size_path), size_info)

  html_report.BuildReportFromSizeInfo(outpath, size_info, all_symbols=False)
  return outpath


def _BuildReports(directory, bucket, skip_existing):
  try:
    if os.listdir(directory):
      raise Exception('Directory must be empty')
  except OSError as e:
    if e.errno == errno.ENOENT:
      os.makedirs(directory)
    else:
      raise

  # GCS URL template used to get size files.
  template = bucket + '/{version}/{cpu}/{apk}.size'

  def GetReportsToMake():
    desired_reports = set(_PossibleReportFiles())
    existing_reports = set(_FetchExistingMilestoneReports())
    missing_reports = desired_reports - existing_reports
    stale_reports = existing_reports - desired_reports
    logging.info('Number of desired reports: %d' % len(desired_reports))
    logging.info('Number of existing reports: %d' % len(existing_reports))
    if stale_reports:
      logging.warning('Number of stale reports: %d' % len(stale_reports))
    if skip_existing:
      logging.info('Generate %d missing reports:' % len(missing_reports))
      return sorted(missing_reports)
    logging.info('Generate all %d desired reports:' %
                 len(desired_reports))
    return sorted(desired_reports)

  reports_to_make = GetReportsToMake()
  if not reports_to_make:
    return

  paths = (_GetReportPaths(directory, template, r) for r in reports_to_make)

  processes = min(len(reports_to_make), multiprocessing.cpu_count())
  pool = multiprocessing.Pool(processes=processes)

  for path in pool.imap_unordered(_BuildReport, paths):
    logging.info('Saved %s', path)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('directory',
                      help='Directory to save report files to '
                           '(must not exist).')
  parser.add_argument('--size-file-bucket', required=True,
                      help='GCS bucket to find size files in.'
                           '(e.g. "gs://bucket/subdir")')
  parser.add_argument('--sync', action='store_true',
                      help='Sync data files to GCS '
                           '(otherwise just prints out command to run).')
  parser.add_argument('--skip-existing', action="store_true",
                      help='Skip existing reports.')
  parser.add_argument('-v',
                      '--verbose',
                      default=0,
                      action='count',
                      help='Verbose level (multiple times for more)')

  args = parser.parse_args()
  logging.basicConfig(level=logging.WARNING - args.verbose * 10,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  size_file_bucket = args.size_file_bucket
  if not size_file_bucket.startswith('gs://'):
    parser.error('Size file bucket must be located in Google Cloud Storage.')
  elif size_file_bucket.endswith('/'):
    # Remove trailing slash
    size_file_bucket = size_file_bucket[:-1]

  _BuildReports(args.directory, size_file_bucket,
                skip_existing=args.skip_existing)
  _SetPushedReports(args.directory)
  logging.warning('Reports saved to %s', args.directory)
  cmd = ['gsutil.py', '-m', 'rsync', '-J', '-a', 'public-read', '-r',
         args.directory, PUSH_URL]

  if args.sync:
    subprocess.check_call(cmd)
  else:
    logging.warning('Sync files by running: \n%s', ' '.join(cmd))


if __name__ == '__main__':
  main()

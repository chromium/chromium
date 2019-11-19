#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for generating Supersize HTML Reports for official builds."""

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import urllib2

_SRC_ROOT = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)

sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android', 'gyp'))
from util import build_utils

_REPORTS_BASE_URL = 'gs://chrome-supersize/official_builds'
_REPORTS_JSON_GS_URL = os.path.join(_REPORTS_BASE_URL, 'reports.json')
_REPORTS_GS_URL = os.path.join(_REPORTS_BASE_URL, 'reports')
_SUPERSIZE_PATH = os.path.join(_SRC_ROOT, 'tools', 'binary_size', 'supersize')


def _FetchReferenceVersion(new_version_str, platform):
  all_versions = json.loads(
      urllib2.urlopen('http://omahaproxy.appspot.com/history.json').read())

  # Filter out versions newer than the last branch point.
  new_version_major = new_version_str.split('.')[0]
  versions = (e['version'] for e in all_versions if e['os'] == platform)
  prev_versions = (e for e in versions if not e.startswith(new_version_major))

  return max(prev_versions, key=lambda x: tuple(int(y) for y in x.split('.')))


def _FetchSizeFileForVersion(gs_url, version, gs_size_path, output_path):
  gs_path = '{}/{}/{}'.format(gs_url, version, gs_size_path)
  cmd = ['gsutil.py', 'cp', gs_path, output_path]
  subprocess.check_call(cmd)


def _CreateReports(report_path, diff_report_path, ref_size_path, size_path):
  subprocess.check_call(
      [_SUPERSIZE_PATH, 'html_report', size_path, report_path])
  subprocess.check_call([
      _SUPERSIZE_PATH, 'html_report', '--diff-with', ref_size_path, size_path,
      diff_report_path
  ])


def _WriteReportsJson(out):
  output = subprocess.check_output(['gsutil.py', 'ls', '-R', _REPORTS_GS_URL])

  reports = []
  report_re = re.compile(
      _REPORTS_GS_URL +
      r'/(?P<cpu>\S+)/(?P<apk>\S+)/(?P<path>report_(?P<version>[^_]+)\.ndjson)')
  for line in output.splitlines():
    m = report_re.search(line)
    if m:
      meta = {
          'cpu': m.group('cpu'),
          'version': m.group('version'),
          'apk': m.group('apk'),
          'path': m.group('path')
      }
      diff_re = re.compile(
          r'{}/{}/(?P<path>report_(?P<version>\S+)_{}.ndjson)'.format(
              meta['cpu'], meta['apk'], meta['version']))
      m = diff_re.search(output)
      if not m:
        raise Exception('Missing diff report for {}'.format(str(meta)))

      meta['diff_path'] = m.group('path')
      meta['reference_version'] = m.group('version')
      reports.append(meta)

  return json.dump({'pushed': reports}, out)


def _UploadReports(reports_json_path, base_url, *ndjson_paths):
  for path in ndjson_paths:
    dst = os.path.join(base_url, os.path.basename(path))
    cmd = ['gsutil.py', 'cp', '-a', 'public-read', path, dst]
    logging.warning(' '.join(cmd))
    subprocess.check_call(cmd)

  with open(reports_json_path, 'w') as f:
    _WriteReportsJson(f)

  cmd = [
      'gsutil.py', 'cp', '-a', 'public-read', reports_json_path,
      _REPORTS_JSON_GS_URL
  ]
  logging.warning(' '.join(cmd))
  subprocess.check_call(cmd)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--version',
      required=True,
      help='Official build version to generate report for (ex. "72.0.3626.7").')
  parser.add_argument(
      '--size-path',
      required=True,
      help='Path to .size file for the given version.')
  parser.add_argument(
      '--gs-size-url',
      required=True,
      help='Bucket url that contains the .size files.')
  parser.add_argument(
      '--gs-size-path',
      required=True,
      help='Path within bucket to a .size file (full path looks like '
      'GS_SIZE_URL/VERSION/GS_SIZE_PATH) used to locate reference .size file.')
  parser.add_argument(
      '--arch', required=True, help='Compiler architecture of build.')
  parser.add_argument(
      '--platform',
      required=True,
      help='OS corresponding to those used by omahaproxy.',
      choices=['android', 'webview'])

  args = parser.parse_args()

  with build_utils.TempDir() as tmp_dir:
    ref_version = _FetchReferenceVersion(args.version, args.platform)
    logging.warning('Found reference version name: %s', ref_version)

    ref_size_path = os.path.join(tmp_dir, ref_version) + '.size'
    report_path = os.path.join(tmp_dir, 'report_{}.ndjson'.format(args.version))
    diff_report_path = os.path.join(
        tmp_dir, 'report_{}_{}.ndjson'.format(ref_version, args.version))
    reports_json_path = os.path.join(tmp_dir, 'reports.json')
    report_basename = os.path.splitext(os.path.basename(args.size_path))[0]
    # Maintain name through transition to bundles.
    report_basename = report_basename.replace('.minimal.apks', '.apk')
    reports_base_url = os.path.join(_REPORTS_GS_URL, args.arch, report_basename)

    _FetchSizeFileForVersion(args.gs_size_url, ref_version, args.gs_size_path,
                             ref_size_path)
    _CreateReports(report_path, diff_report_path, ref_size_path, args.size_path)
    _UploadReports(reports_json_path, reports_base_url, report_path,
                   diff_report_path)


if __name__ == '__main__':
  main()

#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for generating Supersize HTML Reports for official builds."""

import argparse
import json
import logging
import os
import re
import subprocess
import tempfile

_REPORTS_BASE_URL = 'gs://chrome-supersize/official_builds'
_REPORTS_JSON_GS_URL = os.path.join(_REPORTS_BASE_URL, 'canary_reports.json')
_REPORTS_GS_URL = os.path.join(_REPORTS_BASE_URL, 'reports')

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
_GSUTIL = os.path.join(_DIR_SOURCE_ROOT, 'third_party', 'depot_tools',
                       'gsutil.py')


def _WriteReportsJson(out):
  output = subprocess.check_output([_GSUTIL, 'ls', '-R', _REPORTS_GS_URL],
                                   encoding='utf8')

  reports = []
  report_re = re.compile(
      re.escape(_REPORTS_GS_URL) +
      r'/(?P<version>.+?)/(?P<cpu>.+?)/(?P<apk>.+?)\.size')
  for line in output.splitlines():
    m = report_re.search(line)
    if m:
      reports.append({
          'cpu': m.group('cpu'),
          'version': m.group('version'),
          'apk': m.group('apk'),
      })

  json.dump({'pushed': reports}, out)


def _UploadReportsJson():
  with tempfile.NamedTemporaryFile(mode='wt') as f:
    _WriteReportsJson(f)
    f.flush()
    cmd = [
        _GSUTIL, '--', '-h', 'Cache-Control:no-cache', 'cp', '-a',
        'public-read', f.name, _REPORTS_JSON_GS_URL
    ]
    logging.warning(' '.join(cmd))
    subprocess.check_call(cmd)


def _UploadSizeFile(size_path, version, arch):
  report_basename = os.path.splitext(os.path.basename(size_path))[0]
  # Maintain name through transition to bundles.
  report_basename = report_basename.replace('.minimal.apks', '.apk')
  dst_url = os.path.join(_REPORTS_GS_URL, version, arch,
                         report_basename + '.size')

  cmd = [_GSUTIL, 'cp', size_path, dst_url]
  logging.warning(' '.join(cmd))
  subprocess.check_call(cmd)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--version',
      required=True,
      help='Official build version to generate report for (ex. "72.0.3626.7").')
  parser.add_argument('--size-path',
                      required=True,
                      action='append',
                      help='Path to .size file for the given version.')
  parser.add_argument(
      '--arch', required=True, help='Compiler architecture of build.')

  args = parser.parse_args()

  for size_path in args.size_path:
    _UploadSizeFile(size_path, args.version, args.arch)
  _UploadReportsJson()


if __name__ == '__main__':
  main()

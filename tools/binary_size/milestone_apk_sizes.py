#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the large commits given a .csv file from a telemetry size graph."""

import argparse
import collections
import csv
import json
import os
import posixpath
import logging
import multiprocessing.dummy
import subprocess
import sys
import tempfile
import zipfile

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))

sys.path.insert(1, os.path.join(_DIR_SOURCE_ROOT, 'build', 'android', 'gyp'))
from util import zipalign
zipalign.ApplyZipFileZipAlignFix()

_GSUTIL = os.path.join(_DIR_SOURCE_ROOT, 'third_party', 'depot_tools',
                       'gsutil.py')
_RESOURCE_SIZES = os.path.join(_DIR_SOURCE_ROOT, 'build', 'android',
                               'resource_sizes.py')


class _Artifact(object):

  def __init__(self, signed_prefix, name):
    self.name = name
    self._gs_url = posixpath.join(signed_prefix, name)
    self._temp = tempfile.NamedTemporaryFile(suffix=posixpath.basename(name))
    self._path = self._temp.name
    self._resource_sizes_json = None

  def FetchAndMeasure(self):
    args = [_GSUTIL, 'cp', self._gs_url, self._path]
    logging.warning(' '.join(args))
    subprocess.check_call(args)
    args = [
        _RESOURCE_SIZES,
        '--output-format',
        'chartjson',
        '--output-file',
        '-',
        self._path,
    ]
    logging.warning(' '.join(args))
    self._resource_sizes_json = json.loads(subprocess.check_output(args))

  def AddSize(self, metrics):
    metrics[self.name] = self._resource_sizes_json['charts']['InstallSize'][
        'APK size']['value']

  def AddAndroidGoSize(self, metrics):
    metrics[self.name + ' (Android Go)'] = self._resource_sizes_json['charts'][
        'InstallSize']['Estimated installed size (Android Go)']['value']

  def AddMethodCount(self, metrics):
    metrics[self.name + ' (method count)'] = self._resource_sizes_json[
        'charts']['Dex']['unique methods']['value']

  def AddDfmSizes(self, metrics):
    for k, v in sorted(self._resource_sizes_json['charts'].iteritems()):
      if k.startswith('DFM_'):
        metrics['DFM: ' + k[4:]] = v['Size with hindi']['value']

  def PrintLibraryCompression(self):
    with zipfile.ZipFile(self._path) as z:
      for info in z.infolist():
        if info.filename.endswith('.so'):
          sys.stdout.write('{}/{} compressed: {} uncompressed: {}\n'.format(
              self.name, posixpath.basename(info.filename), info.compress_size,
              info.file_size))


def _DumpCsv(metrics):
  csv_writer = csv.DictWriter(
      sys.stdout, fieldnames=metrics.keys(), delimiter='\t')
  csv_writer.writeheader()
  csv_writer.writerow(metrics)


def _DownloadAndAnalyze(signed_prefix):
  artifacts = []

  def make_artifact(name):
    artifacts.append(_Artifact(signed_prefix, name))
    return artifacts[-1]

  chrome = make_artifact('arm/ChromeStable.apk')
  webview = make_artifact('arm/AndroidWebview.apk')
  webview64 = make_artifact('arm_64/AndroidWebview.apk')
  chrome_modern = make_artifact('arm/ChromeModernStable.apks')
  chrome_modern64 = make_artifact('arm_64/ChromeModernStable.apks')
  monochrome = make_artifact('arm/MonochromeStable.apks')
  monochrome64 = make_artifact('arm_64/MonochromeStable.apks')

  # Download in parallel.
  pool = multiprocessing.dummy.Pool()
  pool.map(_Artifact.FetchAndMeasure, artifacts)
  pool.close()

  # Add metrics in the order that we want them in the .csv output.
  metrics = collections.OrderedDict()
  chrome.AddSize(metrics)
  chrome_modern.AddSize(metrics)
  chrome_modern64.AddSize(metrics)
  monochrome.AddSize(metrics)
  monochrome64.AddSize(metrics)
  webview.AddSize(metrics)
  webview64.AddSize(metrics)
  _DumpCsv(metrics)

  metrics = collections.OrderedDict()
  monochrome.AddAndroidGoSize(metrics)
  chrome.AddMethodCount(metrics)
  monochrome.AddMethodCount(metrics)
  monochrome.AddDfmSizes(metrics)
  _DumpCsv(metrics)

  webview.PrintLibraryCompression()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--version', required=True, help='e.g.: "75.0.3770.143"')
  parser.add_argument(
      '--signed-bucket',
      required=True,
      help='GCS bucket to find files in. (e.g. "gs://bucket/subdir")')
  options = parser.parse_args()

  signed_prefix = posixpath.join(options.signed_bucket, options.version)
  _DownloadAndAnalyze(signed_prefix)


if __name__ == '__main__':
  main()

#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the large commits given a .csv file from a telemetry size graph."""

# Our version of pylint doesn't know about python3 yet.
# pylint: disable=unexpected-keyword-arg
import argparse
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
  os.path.join(os.path.dirname(__file__), '../..')
)

sys.path.insert(1, os.path.join(_DIR_SOURCE_ROOT, 'build/android/pylib'))
from utils import app_bundle_utils

_GSUTIL = os.path.join(_DIR_SOURCE_ROOT, 'third_party/depot_tools/gsutil.py')
_RESOURCE_SIZES = os.path.join(
  _DIR_SOURCE_ROOT, 'build/android/resource_sizes.py'
)
_AAPT2 = os.path.join(
  _DIR_SOURCE_ROOT, 'third_party/android_build_tools/aapt2/cipd/aapt2'
)
_KEYSTORE = os.path.join(
  _DIR_SOURCE_ROOT, 'build/android/chromium-debug.keystore'
)
_KEYSTORE_PASSWORD = 'chromium'
_KEYSTORE_ALIAS = 'chromiumdebugkey'


class _Artifact:
  def __init__(self, prefix, name, staging_dir):
    self.name = name
    self._gs_url = posixpath.join(prefix, name)
    self._path = os.path.join(staging_dir, name)
    self._resource_sizes_json = None

    os.makedirs(os.path.dirname(self._path), exist_ok=True)

  def FetchAndMeasure(self):
    args = [_GSUTIL, 'cp', self._gs_url, self._path]
    logging.warning(' '.join(args))
    if not os.path.exists(self._path):
      subprocess.check_call(args)

    path_to_measure = self._path

    if self.name.endswith('.aab'):
      path_to_measure += '.apks'
      app_bundle_utils.GenerateBundleApks(
        self._path,
        path_to_measure,
        _AAPT2,
        _KEYSTORE,
        _KEYSTORE_PASSWORD,
        _KEYSTORE_ALIAS,
        minimal=True,
      )

    args = [
      _RESOURCE_SIZES,
      '--output-format',
      'chartjson',
      '--output-file',
      '-',
      path_to_measure,
    ]
    logging.warning(' '.join(args))
    self._resource_sizes_json = json.loads(subprocess.check_output(args))

  def GetCompressedSize(self):
    return self._resource_sizes_json['charts']['TransferSize'][
      'Transfer size (deflate)'
    ]['value']

  def GetApkSize(self):
    return self._resource_sizes_json['charts']['InstallSize']['APK size'][
      'value'
    ]

  def GetAndroidGoSize(self):
    return self._resource_sizes_json['charts']['InstallSize'][
      'Estimated installed size (Android Go)'
    ]['value']

  def AddSize(self, metrics):
    metrics[self.name] = self.GetApkSize()

  def AddMethodCount(self, metrics):
    metrics[self.name + ' (method count)'] = self._resource_sizes_json[
      'charts'
    ]['Dex']['unique methods']['value']

  def AddDfmSizes(self, metrics, base_name):
    for k, v in sorted(self._resource_sizes_json['charts'].items()):
      if k.startswith('DFM_') and k != 'DFM_test_dummy':
        if k == 'DFM_base':
          name = 'base ({})'.format(base_name)
        else:
          name = k[4:]
        metrics['DFM: ' + name] = v['Size with hindi']['value']


def _DumpCsvAndClear(metrics):
  csv_writer = csv.DictWriter(
    sys.stdout, fieldnames=list(metrics.keys()), delimiter='\t'
  )
  csv_writer.writeheader()
  csv_writer.writerow(metrics)
  metrics.clear()


def _DownloadAndAnalyze(signed_prefix, staging_dir, android_go_system_gz=False):
  artifacts = []

  def make_artifact(name, prefix=signed_prefix):
    artifacts.append(_Artifact(prefix, name, staging_dir))
    return artifacts[-1]

  system_apks = [
    make_artifact('arm/AndroidWebviewSystemStable.apk'),
    make_artifact('arm/ChromeSystemStable.apk'),
  ]
  system_stubs = [
    make_artifact('arm/AndroidWebviewSystemStable-Stub.apk'),
    make_artifact('arm/ChromeSystemStable-Stub.apk'),
  ]

  if not android_go_system_gz:
    webview = make_artifact('arm/AndroidWebviewStable.aab')
    webview64 = make_artifact('arm_64/AndroidWebviewStable.aab')
    webview64_high = make_artifact('high-arm_64/AndroidWebview6432Stable.aab')
    chrome = make_artifact('arm/ChromeStable.aab')
    chrome64 = make_artifact('arm_64/ChromeStable.aab')
    chrome64_high = make_artifact('high-arm_64/ChromeStable.aab')

    system_apks_64 = [
      make_artifact('arm_64/AndroidWebviewSystemStable.apk'),
      make_artifact('arm_64/ChromeSystemStable.apk'),
    ]
    system_apks_64_high = [
      make_artifact('high-arm_64/AndroidWebview64SystemStable.apk'),
      make_artifact('high-arm_64/ChromeSystemStable.apk'),
    ]

  # Download and run resource_sizes.py concurrently.
  pool = multiprocessing.dummy.Pool()
  pool.map(_Artifact.FetchAndMeasure, artifacts)
  pool.close()

  compressed_system_apks_size = sum(x.GetCompressedSize() for x in system_apks)
  stubs_sizes = sum(x.GetApkSize() for x in system_stubs)
  android_go_system_gz_size = compressed_system_apks_size + stubs_sizes
  android_go_system_gz_title = 'Android Go (Trichrome) Compressed System Image'

  if android_go_system_gz:
    print(f'{android_go_system_gz_title}: {android_go_system_gz_size}')
    return

  # Add metrics in the order that we want them in the .csv output.
  metrics = {}
  chrome.AddSize(metrics)
  chrome64.AddSize(metrics)
  chrome64_high.AddSize(metrics)
  webview.AddSize(metrics)
  webview64.AddSize(metrics)
  webview64_high.AddSize(metrics)

  # Separate where spreadsheet has computed columns for easier copy/paste.
  _DumpCsvAndClear(metrics)
  metrics['System Image Size (arm32)'] = sum(
    x.GetApkSize() for x in system_apks
  )
  metrics['System Image Size (arm64)'] = sum(
    x.GetApkSize() for x in system_apks_64
  )
  metrics['System Image Size (arm64-high)'] = sum(
    x.GetApkSize() for x in system_apks_64_high
  )

  go_install_size = chrome.GetAndroidGoSize() + webview.GetAndroidGoSize()
  metrics['Android Go Install Size'] = go_install_size
  metrics[android_go_system_gz_title] = android_go_system_gz_size

  chrome.AddMethodCount(metrics)
  webview.AddMethodCount(metrics)

  # Separate where spreadsheet has computed columns for easier copy/paste.
  _DumpCsvAndClear(metrics)

  chrome64_high.AddDfmSizes(metrics, 'Chrome')
  webview64_high.AddDfmSizes(metrics, 'WebView')
  _DumpCsvAndClear(metrics)


def _CheckGnArgs(unsigned_prefix):
  args = [_GSUTIL, 'cat', unsigned_prefix + '/arm/gn-args-derived.txt']
  logging.warning(' '.join(args))
  gn_args_data = subprocess.check_output(args, text=True)

  def check_arg(name, value):
    if f'{name} = {value}' not in gn_args_data:
      if f'{name} =' not in gn_args_data:
        sys.stderr.write(f'{name} is not in gn-args-derived.txt.\n')
      else:
        sys.stderr.write(f'{name} != {value} in gn-args-derived.txt.\n')
        sys.stderr.write(
          'Sizes will not be accurate. Try again with a later patch version.\n'
        )
      sys.stderr.write('Manually verify via: ' + ' '.join(args) + '\n')
      return False
    return True

  return check_arg('v8_enable_runtime_call_stats', 'false')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--version', required=True, help='e.g.: "75.0.3770.143"')
  parser.add_argument(
    '--signed-bucket',
    required=True,
    help='GCS bucket to find files in. (e.g. "gs://bucket/subdir")',
  )
  parser.add_argument(
    '--keep-files', action='store_true', help='Do not delete downloaded files.'
  )
  parser.add_argument('--android-go-system-gz', action='store_true')
  options = parser.parse_args()

  signed_prefix = posixpath.join(options.signed_bucket, options.version)
  unsigned_prefix = signed_prefix.replace('signed', 'unsigned')

  # Ensure the binary size isn't inflated by v8_enable_runtime_call_stats=true.
  if not _CheckGnArgs(unsigned_prefix):
    if not options.android_go_system_gz:
      sys.exit(1)

  with tempfile.TemporaryDirectory() as staging_dir:
    if options.keep_files:
      staging_dir = 'milestone_apk_sizes-staging'
      os.makedirs(staging_dir, exist_ok=True)

    _DownloadAndAnalyze(
      signed_prefix,
      staging_dir,
      android_go_system_gz=options.android_go_system_gz,
    )

    if options.keep_files:
      print('Saved files to', staging_dir)


if __name__ == '__main__':
  main()

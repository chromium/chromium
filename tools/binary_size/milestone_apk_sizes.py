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
    os.path.join(os.path.dirname(__file__), '../..'))

sys.path.insert(1, os.path.join(_DIR_SOURCE_ROOT, 'build/android/pylib'))
from utils import app_bundle_utils

_GSUTIL = os.path.join(_DIR_SOURCE_ROOT, 'third_party/depot_tools/gsutil.py')
_RESOURCE_SIZES = os.path.join(_DIR_SOURCE_ROOT,
                               'build/android/resource_sizes.py')
_AAPT2 = os.path.join(_DIR_SOURCE_ROOT,
                      'third_party/android_build_tools/aapt2/cipd/aapt2')
_KEYSTORE = os.path.join(_DIR_SOURCE_ROOT,
                         'build/android/chromium-debug.keystore')
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
      app_bundle_utils.GenerateBundleApks(self._path,
                                          path_to_measure,
                                          _AAPT2,
                                          _KEYSTORE,
                                          _KEYSTORE_PASSWORD,
                                          _KEYSTORE_ALIAS,
                                          minimal=True)

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
        'Transfer size (deflate)']['value']

  def GetApkSize(self):
    return self._resource_sizes_json['charts']['InstallSize']['APK size'][
        'value']

  def GetAndroidGoSize(self):
    return self._resource_sizes_json['charts']['InstallSize'][
        'Estimated installed size (Android Go)']['value']

  def AddSize(self, metrics):
    metrics[self.name] = self.GetApkSize()

  def AddMethodCount(self, metrics):
    metrics[self.name + ' (method count)'] = self._resource_sizes_json[
        'charts']['Dex']['unique methods']['value']

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
      sys.stdout, fieldnames=list(metrics.keys()), delimiter='\t')
  csv_writer.writeheader()
  csv_writer.writerow(metrics)
  metrics.clear()


def _DownloadAndAnalyze(signed_prefix,
                        unsigned_prefix,
                        staging_dir,
                        android_go_system_gz=False):
  artifacts = []

  def make_artifact(name, prefix=signed_prefix):
    artifacts.append(_Artifact(prefix, name, staging_dir))
    return artifacts[-1]

  trichrome_system_apks = [
      make_artifact('arm/TrichromeWebViewGoogleSystemStable.apk'),
      make_artifact('arm/TrichromeLibraryGoogleSystemStable.apk'),
      make_artifact(
          'arm/for-signing-only/TrichromeChromeGoogleSystemStable.apk',
          prefix=unsigned_prefix),
  ]
  trichrome_system_stubs = [
      make_artifact('arm/TrichromeWebViewGoogleSystemStubStable.apk'),
      make_artifact('arm/TrichromeLibraryGoogleSystemStubStable.apk'),
      make_artifact(
          'arm/for-signing-only/TrichromeChromeGoogleSystemStubStable.apk',
          prefix=unsigned_prefix),
  ]

  if not android_go_system_gz:
    webview = make_artifact('arm/AndroidWebviewStable.aab')
    webview64 = make_artifact('arm_64/AndroidWebviewStable.aab')
    monochrome = make_artifact('arm/MonochromeStable.aab')
    monochrome64 = make_artifact('arm_64/MonochromeStable.aab')
    trichrome_chrome = make_artifact('arm/TrichromeChromeGoogleStable.aab')
    trichrome_webview = make_artifact('arm/TrichromeWebViewGoogleStable.aab')
    trichrome_library = make_artifact('arm/TrichromeLibraryGoogleStable.apk')
    trichrome64_chrome = make_artifact('arm_64/TrichromeChromeGoogleStable.aab')
    trichrome64_webview = make_artifact(
        'arm_64/TrichromeWebViewGoogleStable.aab')
    trichrome64_library = make_artifact(
        'arm_64/TrichromeLibraryGoogleStable.apk')
    trichrome64_high_chrome = make_artifact(
        'high-arm_64/TrichromeChromeGoogle6432Stable.aab')
    trichrome64_high_webview = make_artifact(
        'high-arm_64/TrichromeWebViewGoogle6432Stable.aab')
    trichrome64_high_library = make_artifact(
        'high-arm_64/TrichromeLibraryGoogle6432Stable.apk')

    trichrome64_system_apks = [
        make_artifact('arm_64/TrichromeWebViewGoogleSystemStable.apk'),
        make_artifact('arm_64/TrichromeLibraryGoogleSystemStable.apk'),
        make_artifact(
            'arm_64/for-signing-only/TrichromeChromeGoogleSystemStable.apk',
            prefix=unsigned_prefix),
    ]
    trichrome64_system_apks_high = [
        make_artifact('high-arm_64/TrichromeWebViewGoogle6432SystemStable.apk'),
        make_artifact('high-arm_64/TrichromeLibraryGoogle6432SystemStable.apk'),
        make_artifact(('high-arm_64/for-signing-only/'
                       'TrichromeChromeGoogle6432SystemStable.apk'),
                      prefix=unsigned_prefix),
    ]

  # Download and run resource_sizes.py concurrently.
  pool = multiprocessing.dummy.Pool()
  pool.map(_Artifact.FetchAndMeasure, artifacts)
  pool.close()

  compressed_system_apks_size = sum(x.GetCompressedSize()
                                    for x in trichrome_system_apks)
  stubs_sizes = sum(x.GetApkSize() for x in trichrome_system_stubs)
  android_go_system_gz_size = compressed_system_apks_size + stubs_sizes
  android_go_system_gz_title = 'Android Go (Trichrome) Compressed System Image'

  if android_go_system_gz:
    print(f'{android_go_system_gz_title}: {android_go_system_gz_size}')
    return

  # Add metrics in the order that we want them in the .csv output.
  metrics = {}
  webview.AddSize(metrics)
  webview64.AddSize(metrics)
  monochrome.AddSize(metrics)
  monochrome64.AddSize(metrics)
  trichrome_chrome.AddSize(metrics)
  trichrome_webview.AddSize(metrics)
  trichrome_library.AddSize(metrics)

  # Separate where spreadsheet has computed columns for easier copy/paste.
  _DumpCsvAndClear(metrics)
  trichrome64_chrome.AddSize(metrics)
  trichrome64_webview.AddSize(metrics)
  trichrome64_library.AddSize(metrics)
  _DumpCsvAndClear(metrics)

  trichrome64_high_chrome.AddSize(metrics)
  trichrome64_high_webview.AddSize(metrics)
  trichrome64_high_library.AddSize(metrics)

  _DumpCsvAndClear(metrics)

  metrics['System Image Size (arm32)'] = sum(x.GetApkSize()
                                             for x in trichrome_system_apks)
  metrics['System Image Size (arm64)'] = sum(x.GetApkSize()
                                             for x in trichrome64_system_apks)
  metrics['System Image Size (arm64-high)'] = sum(
      x.GetApkSize() for x in trichrome64_system_apks_high)

  go_install_size = (trichrome_chrome.GetAndroidGoSize() +
                     trichrome_webview.GetAndroidGoSize() +
                     trichrome_library.GetAndroidGoSize())
  metrics['Android Go (TriChrome) Install Size'] = go_install_size
  metrics[android_go_system_gz_title] = android_go_system_gz_size

  monochrome.AddMethodCount(metrics)

  # Separate where spreadsheet has computed columns for easier copy/paste.
  _DumpCsvAndClear(metrics)

  trichrome_chrome.AddDfmSizes(metrics, 'Chrome')
  trichrome_webview.AddDfmSizes(metrics, 'WebView')
  _DumpCsvAndClear(metrics)


def _CheckGnArgs(unsigned_prefix, version):
  args = [_GSUTIL, 'cat', unsigned_prefix + '/arm/gn-args-derived.txt']
  logging.warning(' '.join(args))
  gn_args_data = subprocess.check_output(args, text=True)

  def check_arg(name, value):
    if f'{name} = {value}' not in gn_args_data:
      if f'{name} =' not in gn_args_data:
        sys.stderr.write(f'{name} is not in gn-args-derived.txt.\n')
      else:
        sys.stderr.write(f'{name} != {value} in gn-args-derived.txt.\n')
        sys.stderr.write('Sizes will not be accurate. Try again with a later '
                         'patch version.\n')
      sys.stderr.write('Manually verify via: ' + ' '.join(args) + '\n')
      return False
    return True

  ret = True
  if int(version.split('.')[0]) < 120:
    if not check_arg('is_on_release_branch', 'true'):
      ret = False
  elif not check_arg('v8_is_on_release_branch', 'true'):
    ret = False

  if not check_arg('v8_enable_runtime_call_stats', 'false'):
    ret = False
  return ret


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--version', required=True, help='e.g.: "75.0.3770.143"')
  parser.add_argument(
      '--signed-bucket',
      required=True,
      help='GCS bucket to find files in. (e.g. "gs://bucket/subdir")')
  parser.add_argument('--keep-files',
                      action='store_true',
                      help='Do not delete downloaded files.')
  parser.add_argument('--android-go-system-gz', action='store_true')
  options = parser.parse_args()

  signed_prefix = posixpath.join(options.signed_bucket, options.version)
  unsigned_prefix = signed_prefix.replace('signed', 'unsigned')

  # Ensure the binary size isn't inflated by v8_is_on_release_branch=true
  # not being set yet.
  if not _CheckGnArgs(unsigned_prefix, options.version):
    if not options.android_go_system_gz:
      sys.exit(1)

  with tempfile.TemporaryDirectory() as staging_dir:
    if options.keep_files:
      staging_dir = 'milestone_apk_sizes-staging'
      os.makedirs(staging_dir, exist_ok=True)

    _DownloadAndAnalyze(signed_prefix,
                        unsigned_prefix,
                        staging_dir,
                        android_go_system_gz=options.android_go_system_gz)

    if options.keep_files:
      print('Saved files to', staging_dir)


if __name__ == '__main__':
  main()

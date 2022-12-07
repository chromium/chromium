#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os

_PATCH = 'cr0'
_LATEST_VERSION = '12.1-robolectric-8229987.' + _PATCH
# All instrumented jars + latest non-instrumented one.
_ROBO_URL_FILES = {
    'android-all-instrumented-12.1-robolectric-8229987-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/12.1-robolectric-8229987-i4/android-all-instrumented-12.1-robolectric-8229987-i4.jar',
    'android-all-instrumented-12-robolectric-7732740-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/12-robolectric-7732740-i4/android-all-instrumented-12-robolectric-7732740-i4.jar',
    'android-all-instrumented-11-robolectric-6757853-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/11-robolectric-6757853-i4/android-all-instrumented-11-robolectric-6757853-i4.jar',
    'android-all-instrumented-10-robolectric-5803371-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/10-robolectric-5803371-i4/android-all-instrumented-10-robolectric-5803371-i4.jar',
    'android-all-instrumented-9-robolectric-4913185-2-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/9-robolectric-4913185-2-i4/android-all-instrumented-9-robolectric-4913185-2-i4.jar',
    'android-all-instrumented-8.1.0-robolectric-4611349-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/8.1.0-robolectric-4611349-i4/android-all-instrumented-8.1.0-robolectric-4611349-i4.jar',
    'android-all-instrumented-8.0.0_r4-robolectric-r1-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/8.0.0_r4-robolectric-r1-i4/android-all-instrumented-8.0.0_r4-robolectric-r1-i4.jar',
    'android-all-instrumented-7.1.0_r7-robolectric-r1-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/7.1.0_r7-robolectric-r1-i4/android-all-instrumented-7.1.0_r7-robolectric-r1-i4.jar',
    'android-all-instrumented-7.0.0_r1-robolectric-r1-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/7.0.0_r1-robolectric-r1-i4/android-all-instrumented-7.0.0_r1-robolectric-r1-i4.jar',
    'android-all-instrumented-6.0.1_r3-robolectric-r1-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/6.0.1_r3-robolectric-r1-i4/android-all-instrumented-6.0.1_r3-robolectric-r1-i4.jar',
    'android-all-instrumented-5.0.2_r3-robolectric-r0-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/5.0.2_r3-robolectric-r0-i4/android-all-instrumented-5.0.2_r3-robolectric-r0-i4.jar',
    'android-all-instrumented-4.4_r1-robolectric-r2-i4.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/4.4_r1-robolectric-r2-i4/android-all-instrumented-4.4_r1-robolectric-r2-i4.jar',
    'android-all-12.1-robolectric-8229987.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/12.1-robolectric-8229987/android-all-12.1-robolectric-8229987.jar'
}

def do_latest():
  print(_LATEST_VERSION)


def _get_download_url(version):
  download_urls, name = [], []
  for robo_name, url in _ROBO_URL_FILES.items():
    name.append(robo_name)
    download_urls.append(url)
  partial_manifest = {
      'url': download_urls,
      'name': name,
      'ext': '.jar',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=lambda _opts: do_latest())

  download = sub.add_parser("get_url")
  download.set_defaults(
      func=lambda _opts: _get_download_url(os.environ['_3PP_VERSION']))

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()


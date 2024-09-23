#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import pathlib

# All instrumented jars + latest non-instrumented one.
_ROBO_URL_FILES = {
    'android-all-14-robolectric-10818077.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all/14-robolectric-10818077/android-all-14-robolectric-10818077.jar',
    'android-all-instrumented-14-robolectric-10818077-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/14-robolectric-10818077-i6/android-all-instrumented-14-robolectric-10818077-i6.jar',
    'android-all-instrumented-13-robolectric-9030017-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/13-robolectric-9030017-i6/android-all-instrumented-13-robolectric-9030017-i6.jar',
    'android-all-instrumented-12.1-robolectric-8229987-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/12.1-robolectric-8229987-i6/android-all-instrumented-12.1-robolectric-8229987-i6.jar',
    'android-all-instrumented-12-robolectric-7732740-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/12-robolectric-7732740-i6/android-all-instrumented-12-robolectric-7732740-i6.jar',
    'android-all-instrumented-11-robolectric-6757853-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/11-robolectric-6757853-i6/android-all-instrumented-11-robolectric-6757853-i6.jar',
    'android-all-instrumented-10-robolectric-5803371-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/10-robolectric-5803371-i6/android-all-instrumented-10-robolectric-5803371-i6.jar',
    'android-all-instrumented-9-robolectric-4913185-2-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/9-robolectric-4913185-2-i6/android-all-instrumented-9-robolectric-4913185-2-i6.jar',
    'android-all-instrumented-8.1.0-robolectric-4611349-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/8.1.0-robolectric-4611349-i6/android-all-instrumented-8.1.0-robolectric-4611349-i6.jar',
    'android-all-instrumented-8.0.0_r4-robolectric-r1-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/8.0.0_r4-robolectric-r1-i6/android-all-instrumented-8.0.0_r4-robolectric-r1-i6.jar',
    'android-all-instrumented-7.1.0_r7-robolectric-r1-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/7.1.0_r7-robolectric-r1-i6/android-all-instrumented-7.1.0_r7-robolectric-r1-i6.jar',
    'android-all-instrumented-7.0.0_r1-robolectric-r1-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/7.0.0_r1-robolectric-r1-i6/android-all-instrumented-7.0.0_r1-robolectric-r1-i6.jar',
    'android-all-instrumented-6.0.1_r3-robolectric-r1-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/6.0.1_r3-robolectric-r1-i6/android-all-instrumented-6.0.1_r3-robolectric-r1-i6.jar',
    'android-all-instrumented-5.0.2_r3-robolectric-r0-i6.jar':
        'https://repo1.maven.org/maven2/org/robolectric/android-all-instrumented/5.0.2_r3-robolectric-r0-i6/android-all-instrumented-5.0.2_r3-robolectric-r0-i6.jar',
}


def do_latest():
  # Make the version change every time this file changes.
  md5 = hashlib.md5()
  md5.update(pathlib.Path(__file__).read_bytes())
  file_hash = md5.hexdigest()[:10]
  # Prefix with the first version from the dict, which should be the
  # non-instrumented .jar, to make the version string not entirely random.
  first_file = next(iter(_ROBO_URL_FILES))
  assert '-instrumented' not in first_file and first_file.endswith('.jar')
  first_file = first_file[:-len('.jar')]
  print(f'{first_file}-{file_hash}')


def do_get_url():
  partial_manifest = {
      'url': list(_ROBO_URL_FILES.values()),
      'name': list(_ROBO_URL_FILES),
      'ext': '.jar',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers(required=True)

  latest = sub.add_parser('latest')
  latest.set_defaults(func=lambda _opts: do_latest())

  download = sub.add_parser('get_url')
  download.set_defaults(func=lambda _opts: do_get_url())

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()

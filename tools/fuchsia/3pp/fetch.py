#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request

_GIT_HOST = 'https://chromium.googlesource.com/chromium/src.git/'
_COMMIT_URL = _GIT_HOST + '+log/refs/heads/main/build/fuchsia?n=1&format=json'
_ARCHIVE_URL = _GIT_HOST + '+archive/{}/build/fuchsia.tar.gz'


def get_last_commit():
  """Returns the last commit of build/fuchsia.
     The approach is not very ideal since we may want to ignore unrelated
     changes like the linux_internal.sdk.sha1 file.
  """
  content = urllib.request.urlopen(_COMMIT_URL).read().decode('utf-8')
  # Remove the leading json safety prefix.
  assert content.startswith(")]}'")
  return json.loads(content[4:])['log'][0]['commit']


def do_latest():
  print(get_last_commit())


def get_download_url(sha):
  """Returns the manifest for the following download procedure."""
  partial_manifest = {
      'url': [_ARCHIVE_URL.format(sha)],
      'ext': '.tar.gz',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=do_latest)

  download = sub.add_parser("get_url")
  download.set_defaults(func=lambda: get_download_url(
      os.environ.get('_3PP_VERSION', get_last_commit())))

  opts = ap.parse_args()
  opts.func()


if __name__ == '__main__':
  main()

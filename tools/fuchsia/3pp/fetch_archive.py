#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request


def _get_last_commit(commit_url):
  """Returns the last commit of build/fuchsia.
     The approach is not very ideal since we may want to ignore unrelated
     changes like the linux_internal.sdk.sha1 file.
  """
  content = urllib.request.urlopen(commit_url).read().decode('utf-8')
  # Remove the leading json safety prefix.
  assert content.startswith(")]}'")
  return json.loads(content[4:])['log'][0]['commit']


def _do_latest(commit_url):
  print(_get_last_commit(commit_url))


def _get_download_url(sha, archive_url):
  """Returns the manifest for the following download procedure."""
  partial_manifest = {
      'url': [archive_url.format(sha)],
      'ext': '.tar.gz',
  }
  print(json.dumps(partial_manifest))


def main(commit_url, archive_url):
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=lambda: _do_latest(commit_url))

  download = sub.add_parser("get_url")
  download.set_defaults(func=lambda: _get_download_url(
      os.environ.get('_3PP_VERSION', _get_last_commit(commit_url)),
      archive_url))

  opts = ap.parse_args()
  opts.func()

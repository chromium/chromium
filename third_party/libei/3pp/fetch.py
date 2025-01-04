#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import urllib.request

def latest():
  request = urllib.request.Request('https://gitlab.freedesktop.org/libinput/'
                                   'libei/-/releases/permalink/latest',
                                   method='HEAD')
  response = urllib.request.urlopen(request)
  return response.url.split('/')[-1]


def do_latest(*args, **kwargs):
  print(latest())


def get_download_url(*args, **kwargs):
  version = latest()

  urls = [
      "https://gitlab.freedesktop.org/libinput/libei/-/"
      f"archive/{version}/libei-{version}.tar.gz",
      "https://github.com/WayneD/rsync/archive/refs/tags/v3.2.7.tar.gz",
      "https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.4.114.tar.gz",
  ]

  packages = [
      f"libei-{version}.tar.gz",
      "v3.2.7.tar.gz",  # rsync tar ball
      "linux-5.4.114.tar.gz",
  ]

  partial_manifest = {
      'url': urls,
      'name': packages,
      'ext': '.tar.gz',
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest_parser = sub.add_parser("latest")
  latest_parser.set_defaults(func=do_latest)

  download_parser = sub.add_parser("get_url")
  download_parser.set_defaults(func=get_download_url)

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()

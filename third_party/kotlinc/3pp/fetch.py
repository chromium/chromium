#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os

_FILE_URL = 'https://github.com/JetBrains/kotlin/releases/download/v{0}/kotlin-compiler-{0}.zip'
_FILE_NAME = 'kotlinc.zip'
_VERSION = '1.7.21'
_PATCH_VERSION = '-cr1'


def do_latest():
    print(_VERSION + _PATCH_VERSION)


def get_download_url(version):
    if version.endswith(_PATCH_VERSION):
      version = version[:-len(_PATCH_VERSION)]
    partial_manifest = {
        'url': [_FILE_URL.format(version)],
        'name': [_FILE_NAME],
        'ext': '.zip',
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=lambda _opts: do_latest())

    download = sub.add_parser("get_url")
    download.set_defaults(
        func=lambda _opts: get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func(opts)


if __name__ == '__main__':
    main()

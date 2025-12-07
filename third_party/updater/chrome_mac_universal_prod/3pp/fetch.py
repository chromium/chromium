#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import json
import os
import sys
import urllib.request

# TODO(crbug.com/1336630): This is compared lexically. Remove it before M1000.
MIN_VERSION = '129.0.6651.0'

def fetch():
    """
    Queries the VersionHistory API to determine the version of the updater that
    is serving with a fraction of 1.
  """
    return json.load(
        urllib.request.urlopen(
            'https://versionhistory.googleapis.com/v1/chromium_updater/'
            'platforms/mac/channels/all/versions/all/releases?'
            'filter=fraction=1'))['releases'][0]['version']


def print_latest():
    print(max(fetch(), MIN_VERSION))


def get_url():
    print(
        json.dumps({
            'url': [
                'https://edgedl.me.gvt1.com/edgedl/release2/182l0/latest/'
                'GoogleUpdater-%s.zip' % os.environ['_3PP_VERSION']
            ],
            'ext': '.zip',
            'name': ['GoogleUpdater-%s.zip' % os.environ['_3PP_VERSION']]
        }))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()
    sub.add_parser('latest').set_defaults(func=lambda _opts: print_latest())
    sub.add_parser('get_url').set_defaults(func=lambda _opts: get_url())
    opts = ap.parse_args()
    return opts.func(opts)


if __name__ == '__main__':
    sys.exit(main())

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

# TODO(crbug.com/1268555): This is compared lexically. Remove it before M1000.
MIN_VERSION = '119.0.5999.0'

def fetch():
    """
    Queries the VersionHistory API to determine the version of the updater that
    was serving on Monday.
  """
    # TODO(crbug.com/1293206): Once this script is python3 only, use
    # datetime.timezone.utc to make it consistent regardless of local timezone.
    datum = datetime.datetime.now()
    datum = (datum - datetime.timedelta(days=datum.weekday())).replace(
        hour=0, minute=0, second=0, microsecond=0)
    datum = datum.isoformat() + 'Z'

    return json.load(
        urllib.request.urlopen(
            'https://versionhistory.googleapis.com/v1/chromium_updater/'
            'platforms/win/channels/all/versions/all/releases?'
            'filter=starttime%%3C%s,endtime%%3E%s' %
            (datum, datum)))['releases'][0]['version']


def print_latest():
    print(max(fetch(), MIN_VERSION))


def get_url():
    print(
        json.dumps({
            'url': [
                'https://edgedl.me.gvt1.com/edgedl/release2/182l0/latest/'
                '%s_win64_updater.zip' % os.environ['_3PP_VERSION']
            ],
            'ext':
            '.zip',
            'name': ['updater.zip']
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

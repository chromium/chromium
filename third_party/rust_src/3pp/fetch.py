#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import json
import os
import sys
import urllib.request as rq

# Placeholder filled in with an ISO 8601 format date (YYYY-MM-DD).
_PACKAGE_URL = 'https://static.rust-lang.org/dist/{}/rustc-nightly-src.tar.xz'
_MAX_DATES_TO_CHECK = 10

def find_latest_release_date():
    cur_date = datetime.datetime.today().date()

    for i in range(0, _MAX_DATES_TO_CHECK):
        if upstream_has_package(cur_date):
            return cur_date
        cur_date = cur_date - datetime.timedelta(days=1)

    return None

def upstream_has_package(date):
    # Make HEAD request to check package existence without downloading it.
    req = rq.Request(package_url_for_date(date), method='HEAD')
    with rq.urlopen(req) as resp:
        return resp.getcode() == 200

def package_url_for_date(date):
    return _PACKAGE_URL.format(date.isoformat())

def do_latest():
    print(find_latest_release_date())

def do_get_url():
    version = os.environ['_3PP_VERSION']
    print(json.dumps(
        {
            'url': [_PACKAGE_URL.format(version)],
            'ext': '.tar.xz',
            'name': ['rustc-nightly-src.tar.xz'],
        }
    ))

def main():
    argp = argparse.ArgumentParser()
    subp = argp.add_subparsers(required=True)

    latest = subp.add_parser("latest")
    latest.set_defaults(func = do_latest)

    get_url = subp.add_parser("get_url")
    get_url.set_defaults(func = do_get_url)

    args = argp.parse_args()
    args.func()

    return 0

if __name__ == '__main__':
    sys.exit(main())

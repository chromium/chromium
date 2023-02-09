#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import functools
import json
import math
import os
import sys
import urllib.request

# MIN_VERSION is the earliest working version of the updater for self-update
# testing. If a backwards-incompatible change to the updater is made, it may be
# necessary to increase the version.
MIN_VERSION = 1103373

def get_platform():
    return 'Win'


def int_or_inf(v):
    try:
        return int(v)
    except ValueError:
        return -float('inf')


def fetch(platform, minimum, minimum_lexographic):
    """
    Queries GCS for versions and returns a tuple (min, max), where min is the
    (numerically) lowest version greater than `minimum` returned by GCS, and
    max is the greatest (lexographically) version returned by GCS. Because GCS
    compares and returns items in lexographic order, GCS may return no eligible
    min items. (For example, if minimum = 200, it could return 30, 31, 32...)
    In this case, min will be float('inf') and the caller should query with max
    as the new minimum_lexographic.
  """
    return functools.reduce(
        lambda a, b: (min(a[0], int(b)) if int_or_inf(b) > minimum else a[0],
                      max(a[1], b)),
        map(
            lambda s: s[len(platform) + 1:-1],
            json.load(
                urllib.request.urlopen(
                    'https://storage.googleapis.com/storage/v1/b/'
                    'chromium-browser-snapshots/o?prefix=%s%%2F&startOffset=%s'
                    '%%2F%s&fields=prefixes&delimiter=%%2F' %
                    (platform, platform,
                     minimum_lexographic)))['prefixes']),
        (float('inf'), ''))


def find(platform, minimum, maximum):
    """
    Returns a version from GCS closest to `minimum` but not more than `maximum`
    for `platform`. May return maximum even if it does not exist in GCS.
  """
    found_min = maximum
    pivot = str(minimum)
    while pivot < str(maximum):
        found, pivot = fetch(platform, minimum, pivot)
        found_min = min(found_min, found)
    return found_min


def lastDatum(platform):
    """
    Returns a version from GCS that only updates every n versions.
    """
    latest = int(
        urllib.request.urlopen(
            'https://storage.googleapis.com/storage/v1/b/'
            'chromium-browser-snapshots/o/%s%%2FLAST_CHANGE?alt=media' % platform).read())
    return max(MIN_VERSION,
               find(platform, latest - latest % 1000, latest))


def print_latest():
    print(lastDatum(get_platform()))


def get_url():
    print(
        json.dumps({
            'url': [
                'https://storage.googleapis.com/storage/v1/b/'
                'chromium-browser-snapshots/o/%s%%2F%s%%2Fupdater.zip?alt=media'
                % (get_platform(), os.environ['_3PP_VERSION'])
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

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import functools
import json
import math
import os
import sys
import urllib.request
import enum


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
    In this case, min will be float('inf') and the caller should query with max as
    the new minimum_lexographic.
  """
    return functools.reduce(
        lambda a, b: (min(a[0], int(b))
                      if int_or_inf(b) > minimum else a[0], max(a[1], b)),
        map(
            lambda s: s[len(platform) + 1:-1],
            json.load(
                urllib.request.urlopen(
                    'https://storage.googleapis.com/storage/v1/b/'
                    'chromium-browser-snapshots/o?prefix=%s%%2F&startOffset=%s'
                    '%%2F%s&fields=prefixes&delimiter=%%2F' %
                    (platform, platform, minimum_lexographic)))['prefixes']),
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


def lastDatum(platform, min_version):
    """
    Returns a version from GCS that only updates every n versions.
    """
    latest = int(
        urllib.request.urlopen(
            'https://storage.googleapis.com/storage/v1/b/'
            'chromium-browser-snapshots/o/%s%%2FLAST_CHANGE?alt=media' %
            platform).read())
    return max(min_version, find(platform, latest - latest % 1000, latest))


def print_latest(platform, min_version):
    print(lastDatum(platform, min_version))


def get_url(platform):
    print(
        json.dumps({
            'url': [
                'https://storage.googleapis.com/storage/v1/b/'
                'chromium-browser-snapshots/o/%s%%2F%s%%2Fenterprise_companion_test.zip?alt=media'
                % (platform, os.environ['_3PP_VERSION'])
            ],
            'ext':
            '.zip',
            'name': ['enterprise_companion_test.zip']
        }))


def main(platform, min_version):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()
    sub.add_parser('latest').set_defaults(
        func=lambda _opts: print_latest(platform, min_version))
    sub.add_parser('get_url').set_defaults(
        func=lambda _opts: get_url(platform))
    opts = ap.parse_args()
    return opts.func(opts)

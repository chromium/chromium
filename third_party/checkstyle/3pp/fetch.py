#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request

_GITHUB_URL = 'https://api.github.com/repos/checkstyle/checkstyle'
_RELEASES_URL = f'{_GITHUB_URL}/releases'


def fetch_json(url):
    return json.load(urllib.request.urlopen(url))


def do_latest():
    json_dict = fetch_json(f'{_RELEASES_URL}/latest')
    print(json_dict['tag_name'])


def get_download_url():
    version = os.environ['_3PP_VERSION']
    json_dict = fetch_json(f'{_RELEASES_URL}/tags/{version}')
    # There is currently just one asset, but do a quick filter to guard against
    # possible future assets.
    urls = [x['browser_download_url'] for x in json_dict['assets']]
    urls = [x for x in urls if '-all' in x and x.endswith('.jar')]
    if len(urls) != 1:
        raise Exception('len(urls) != 1: ' + '\n'.join(urls))

    partial_manifest = {
        'url': urls,
        # Used only if recipe needs to extract.
        'ext': '',
        # Use constant filename to avoid updating pathswhen rolled.
        'name': ['checkstyle-all.jar'],
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    latest = sub.add_parser('latest')
    latest.set_defaults(func=do_latest)

    download = sub.add_parser('get_url')
    download.set_defaults(func=get_download_url)

    ap.parse_args().func()


if __name__ == '__main__':
    main()

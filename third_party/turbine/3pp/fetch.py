#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import re
import urllib

_COMMITS_URL = 'https://api.github.com/repos/google/turbine/commits?per_page=1'
_ARCHIVE_URL = 'https://github.com/google/turbine/archive/{}.tar.gz'


def get_last_commit_data():
    return json.loads(urllib.urlopen(_COMMITS_URL).read().strip())


def do_latest():
    message = get_last_commit_data()[0]['commit']['message']
    rev_id_match = re.search(r'PiperOrigin-RevId: (\d+)', message)
    assert rev_id_match is not None, (
        'Unable to find PiperOrigin-RevId in commit message:' + message)
    print(rev_id_match.group(1))


def get_download_url():
    sha = get_last_commit_data()[0]['sha']
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
    download.set_defaults(func=get_download_url)

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()

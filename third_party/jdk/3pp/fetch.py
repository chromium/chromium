#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import os
import pathlib
import urllib.request

_LATEST_URL = 'https://api.github.com/repos/adoptium/temurin20-binaries/releases/latest'


def get_latest_release():
    """Returns data of the latest release."""
    return json.load(urllib.request.urlopen(_LATEST_URL))


def do_latest():
    # Make the version change every time this file changes.
    md5 = hashlib.md5()
    md5.update(pathlib.Path(__file__).read_bytes())
    file_hash = md5.hexdigest()[:10]
    release = get_latest_release()
    print('{}.{}'.format(release['name'], file_hash))


def do_get_url(version_with_hash):
    release = get_latest_release()
    latest_release_name = release['name']
    version_release_name = version_with_hash.rsplit('.', 1)[0]
    assert latest_release_name == version_release_name, (
        f'The latest release name is {latest_release_name} but the one given '
        f'via the version parameter is {version_release_name}.')

    # version without file hash.
    version = release['name']
    release_url = None
    for asset in release['assets']:
        name = asset['name']
        if 'jdk_x64_linux_' in name and name.endswith('.tar.gz'):
            if release_url is not None:
                raise Exception(
                    'Multiple valid assets found: \n{}\n{}\n'.format(
                        release_url, asset['browser_download_url']))
            release_url = asset['browser_download_url']

    if release_url is None:
        raise Exception(f'jdk_x64_linux asset not found for {version}.')

    partial_manifest = {
        'url': [release_url],
        'ext': '.tar.gz',
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    latest = sub.add_parser("latest")
    latest.set_defaults(func=do_latest)

    download = sub.add_parser("get_url")
    download.set_defaults(func=lambda: do_get_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()

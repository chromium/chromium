#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import os
import pathlib
import urllib.request

# Releases can be found: https://github.com/JetBrains/kotlin/releases/
_LATEST_URL = 'https://api.github.com/repos/JetBrains/kotlin/releases/latest'


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


def get_download_url(original_version):
    release = get_latest_release()
    latest_release_name = release['name']
    version_release_name = original_version.rsplit('.', 1)[0]
    assert latest_release_name == version_release_name, (
        f'The latest release name is {latest_release_name} but the one given '
        f'via the version parameter is {version_release_name}.')

    # version without file hash.
    version = release['name']
    release_url = None
    file_name = None
    for asset in release['assets']:
        name = asset['name']
        if 'kotlin-compiler-' in name and name.endswith('.zip'):
            if release_url is not None:
                raise Exception(
                    'Multiple valid assets found: \n{}\n{}\n'.format(
                        release_url, asset['browser_download_url']))
            file_name = name
            release_url = asset['browser_download_url']

    if release_url is None:
        raise Exception(f'kotlin-compiler asset not found for {version}.')

    partial_manifest = {
        'url': [release_url],
        'name': [file_name],
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

#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request

PACKAGE_NAME = "wtl"

def get_latest_stable_version():
    url = (f"https://api.nuget.org/v3-flatcontainer/"
           f"{PACKAGE_NAME.lower()}/index.json")
    with urllib.request.urlopen(url) as response:
        data = json.loads(response.read().decode())
        versions = data['versions']

        # Filter out any pre-release versions.
        stable_versions = [v for v in versions if '-' not in v]

        if not stable_versions:
            raise Exception("No stable versions found for " + PACKAGE_NAME)

        return stable_versions[-1]

def get_download_url(version):
    download_url = (f"https://www.nuget.org/api/v2/package/"
                    f"{PACKAGE_NAME.lower()}/{version}")

    partial_manifest = {
        'url': [download_url],
        'ext': '.zip',
    }
    print(json.dumps(partial_manifest))

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=lambda _opts: print(get_latest_stable_version()))

    download = sub.add_parser("get_url")
    download.set_defaults(
        func=lambda _opts: get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func(opts)

if __name__ == '__main__':
    main()

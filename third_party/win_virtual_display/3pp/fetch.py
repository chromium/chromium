#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request

_GIT_HOST = "https://chromium.googlesource.com/chromium/src.git/"
_COMMIT_URL = _GIT_HOST + "+log/refs/heads/main/third_party/win_virtual_display?n=1&format=json"
_ARCHIVE_URL = _GIT_HOST + "+archive/{}/third_party/win_virtual_display.tar.gz"


def get_last_commit():
    """Returns the last commit of third_party/win_virtual_display."""
    content = urllib.request.urlopen(_COMMIT_URL).read().decode("utf-8")
    # Remove the leading json safety prefix.
    assert content.startswith(")]}'")
    return json.loads(content[4:])["log"][0]["commit"]


def get_download_url(sha):
    """Returns the manifest for the following download procedure."""
    partial_manifest = {
        "url": [_ARCHIVE_URL.format(sha)],
        "ext": ".tar.gz",
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=lambda: print(get_last_commit()))

    download = sub.add_parser("get_url")
    download.set_defaults(func=lambda: get_download_url(
        os.environ.get("_3PP_VERSION", get_last_commit())))

    ap.parse_args().func()


if __name__ == "__main__":
    main()

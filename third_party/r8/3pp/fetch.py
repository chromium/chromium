#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import urllib.request

# I have arbitrarily chosen 100 as a number much more than the number of commits
# I expect to see in a day in R8.
_COMMITS_URL = 'https://r8.googlesource.com/r8/+log/HEAD~100..HEAD?format=JSON'
_ARCHIVE_URL = 'https://r8.googlesource.com/r8/+archive/{}.tar.gz'


def get_commit_before_today():
    """Returns hash of last commit that occurred before today (in Hawaii time)."""
    # We are doing UTC-10 (Hawaii time) since midnight there is roughly equal to a
    # time before North America starts working (where much of the chrome build
    # team is located), and is at least partially through the workday for Denmark
    # (where much of the R8 team is located). This ideally allows 3pp to catch
    # R8's work faster, instead of doing UTC where we would typically have to wait
    # ~18 hours to get R8's latest changes.
    desired_timezone = datetime.timezone(-datetime.timedelta(hours=10))
    today = datetime.datetime.now(desired_timezone).date()
    # Response looks like:
    # {
    # "log": [
    #   {
    #     "commit": "90f65f835fa73e2de31eafd6784cc2a0580dbe80",
    #     "committer": {
    #       "time": "Mon May 02 11:18:05 2022 +0000"
    #       ...
    #     },
    #     ...
    #   },
    #   ...
    # ]}
    response_string = urllib.request.urlopen(_COMMITS_URL).read()
    # JSON response has a XSSI protection prefix )]}'
    parsed_response = json.loads(response_string.lstrip(b")]}'\n"))
    log = parsed_response['log']
    for commit in log:
        # These times are formatted as a ctime() plus a time zone at the end.
        # "Mon May 02 13:09:58 2022 +0200"
        ctime_string = commit['committer']['time']
        commit_time = datetime.datetime.strptime(ctime_string,
                                                 "%a %b %d %H:%M:%S %Y %z")
        normalized_commit_time = commit_time.astimezone(desired_timezone)

        # We are assuming that the commits are given in order of committed time.
        # This appears to be true, but I can't find anywhere that this is
        # guaranteed.
        if normalized_commit_time.date() < today:
            # This is the first commit we can find before today.
            return commit['commit']

    # Couldn't find any commits before today - should probably change the
    # _COMMITS_URL to get more than 100 commits.
    return None


def compute_patch_hash():
    this_dir = pathlib.Path(__file__).parent
    md5 = hashlib.md5()
    for p in sorted(this_dir.glob('patches/*.patch')):
        md5.update(p.read_bytes())
    # Include install.py so that it triggers changes as well.
    md5.update((this_dir / 'install.sh').read_bytes())
    # Shorten to avoid really long version strings. Given the low number of patch
    # files, 10 digits is more than sufficient.
    return md5.hexdigest()[:10]


def do_latest():
    commit_hash = get_commit_before_today()
    assert commit_hash is not None
    patch_hash = compute_patch_hash()
    # Include hash of patch files so that 3pp bot will create a new version when
    # they change.
    print(f'{commit_hash}-{patch_hash}')


def get_download_url(version):
    sha = version.split('-')[0]
    partial_manifest = {
        'url': [_ARCHIVE_URL.format(sha)],
        'ext': '.tar.gz',
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    latest = sub.add_parser("latest")
    latest.set_defaults(func=do_latest)

    download = sub.add_parser("get_url")
    download.set_defaults(
        func=lambda: get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()

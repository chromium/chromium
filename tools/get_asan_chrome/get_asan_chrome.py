#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download ASAN Chrome - Helper for downloading an ASAN build of Chrome.

Uses Chromium Dash to fetch the latest build info for all supported platforms.
Then tries to find the corresponding ASAN build (or one close to it) from
Chrome's GCS bucket of ASAN builds.
"""

import argparse
import datetime
import json
import logging
import os
import platform
import sys
from urllib.request import urlretrieve
from urllib.request import urlopen
from urllib.parse import quote as urlquote
from urllib import error as urlliberror


class ChromeRelease:

    def __init__(self, os, branch_position, version, channel) -> None:
        self.os = os
        self.branch_position = branch_position
        self.version = version
        self.channel = channel


def get_current_os():
    # Translates platform.system() values to corresponding Chromium Dash names.
    return {
        'Windows': 'win64',
        'Linux': 'linux',
        'Darwin': 'mac',
    }[platform.system()]


def fail(msg):
    logging.fatal(msg)
    logging.shutdown()
    exit()


def fetch_json(release_info_url):
    logging.debug(f'Fetching JSON release metadata from {release_info_url}')
    with urlopen(release_info_url) as resp:
        if resp.status != 200:
            fail(f'Failed to fetch Chromium release data from '
                 f'{release_info_url}')
        try:
            return json.loads(resp.read())
        except json.JSONDecodeError:
            fail(f'Failed to parse release metadata response from '
                 f'{release_info_url}')


def get_release_metadata_by_version(version):
    uri = (f'https://chromiumdash.appspot.com/fetch_version'
           f'?version={version}')
    json_response = fetch_json(uri)
    return json_response['chromium_main_branch_position']


def get_release_metadata_by_channel(release_info):
    os_to_platform = {
        'linux': 'linux',
        'linux_debug': 'linux',
        'mac': 'mac',
        'win64': 'win64',
        'lacros': 'lacros',
    }
    platform = os_to_platform[release_info.os]
    uri = (f'https://chromiumdash.appspot.com/fetch_releases'
           f'?platform={platform}'
           f'&channel={release_info.channel}'
           f'&num=1&offset=0')
    json_response = fetch_json(uri)
    if not json_response:
        fail(f'No releases found for platform "{platform}" '
             f'and channel "{release_info.channel}"')
    release = json_response[0]
    release_info.branch_position = release['chromium_main_branch_position']
    release_info.version = release['version']


def get_release_metadata(release_info):
    # If the OS unspecified, detect and use the current OS.
    if not release_info.os:
        release_info.os = get_current_os()
    if release_info.branch_position:
        return
    elif release_info.version:
        release_info.branch_position = get_release_metadata_by_version(
            release_info.version)
    else:
        # If the channel unspecified, use channel closest to ToT for given OS.
        if not release_info.channel:
            if release_info.os == 'linux':
                # Linux doesn't have a canary channel.
                release_info.channel = 'dev'
            else:
                release_info.channel = 'canary'
        get_release_metadata_by_channel(release_info)


def download_asan_chrome(release_info, download_dir, quiet, retries=100):

    def report_hook(blocknum, blocksize, totalsize):
        if quiet:
            return
        size = blocknum * blocksize
        if totalsize == -1:  # Total size not known.
            progress = f'Received {size} bytes'
        else:
            size = min(totalsize, size)
            percent = 100 * size / totalsize
            progress = f'Received {size} of {totalsize} bytes, {percent:.2f}%'
        # Send a \r to let all progress messages use just one line of output.
        sys.stdout.write('\r' + progress)
        sys.stdout.flush()

    # Translates Chromium Dash OS names to corresponding GCS storage paths.
    os_to_path = {
        'win64': 'win32-release_x64/asan-win32-release_x64',
        'linux': 'linux-release/asan-linux-release',
        'linux_debug': 'linux-debug/asan-linux-debug',
        'mac': 'mac-release/asan-mac-release',
        # 'mac_debug': 'mac-debug/asan-mac-debug',
        # 'ios': 'ios-release/asan-ios-release', # unsupported
        'lacros': 'linux-release-chromeos/asan-linux-release',
        # android is currently unsupported
    }

    if retries < 1:
        fail('Exceeded retry limit, aborting.')

    path = urlquote(os_to_path[release_info.os], safe='')
    asan_build_uri = (f'https://www.googleapis.com/download/storage/v1/b/'
                      f'chromium-browser-asan/o/{path}-'
                      f'{release_info.branch_position}.zip?alt=media')
    if release_info.version:
        outfile_name = (f'chromium-{release_info.version}'
                        f'-{release_info.os}-asan.zip')
    else:
        outfile_name = (f'chromium-{release_info.branch_position}-'
                        f'{release_info.os}-asan.zip')
    outfile_path = os.path.join(download_dir, outfile_name)
    try:
        logging.debug(f'Fetching ASAN build from {asan_build_uri}')
        outfile_path, _ = urlretrieve(asan_build_uri, outfile_path,
                                      report_hook)
    except urlliberror.HTTPError as e:
        if e.code == 404 and retries > 0:
            # Not every branch position gets an ASAN build, so try the previous
            # branch position and hope for the best.
            new_branch_position = str(int(release_info.branch_position) - 1)
            logging.warning(
                f'No ASAN build for {release_info.os} at branch position '
                f'{release_info.branch_position}, retrying at position '
                f'{new_branch_position}...')
            release_info.branch_position = new_branch_position
            if os.path.exists(outfile_path):
                os.unlink(outfile_path)
            download_asan_chrome(release_info, download_dir, quiet,
                                 retries - 1)
        else:
            fail(f'Failed fetching build from {asan_build_uri}: {e}')


def main(release_info, download_dir, quiet):
    get_release_metadata(release_info)
    download_asan_chrome(release_info, download_dir, quiet)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--version',
                       help='Chrome version, e.g. 120.0.6099.216.')
    group.add_argument('--branch_position',
                       help='Chrome branch base position, e.g. 1025959.')
    group.add_argument('--channel',
                       choices=['canary', 'dev', 'beta', 'stable'],
                       help='Chromium channel, e.g. canary.')
    parser.add_argument(
        '--os',
        choices=[
            'win64',
            'linux',
            'linux_debug',
            'mac',
            'lacros',
        ],
        help='Operating system type as defined by Chromium Dash.')
    parser.add_argument(
        '--download_directory',
        default='.',
        help='Path of directory where downloaded ASAN build will be saved.')
    parser.add_argument('--save_log',
                        help='Save activity log to disk.',
                        action='store_true',
                        default=False)
    parser.add_argument(
        '--quiet',
        help='Decrease log output and don\'t show download progress.',
        action='store_true',
        default=False)
    args = parser.parse_args()

    loglevel = logging.INFO
    if args.quiet:
        log = logging.WARN
    if args.save_log:
        logfile_name = os.path.basename(__file__).strip(
            '.py') + '-' + datetime.datetime.now().strftime('%Y%m%d') + '.log'
        stdout_handler = logging.FileHandler(filename=logfile_name)
        stderr_handler = logging.StreamHandler(sys.stderr)
        logging.basicConfig(level=loglevel,
                            handlers=[stdout_handler, stderr_handler])
    else:
        logging.basicConfig(level=loglevel)

    release_info = ChromeRelease(args.os, args.branch_position, args.version,
                                 args.channel)
    main(release_info, args.download_directory, args.quiet)

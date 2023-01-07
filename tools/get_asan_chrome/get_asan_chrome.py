#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download ASAN Chrome - Helper for downloading an ASAN build of Chrome.

Uses Omaha Proxy to fetch latest build info for all supported platforms. Then
tries to find the corresponding ASAN build (or one close to it) from Chrome's
GCS bucket of ASAN builds.
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


class ChromeRelease():
    def __init__(self, os, branch_position, version, channel,
                 true_branch) -> None:
        self.os = os
        self.branch_position = branch_position
        self.version = version
        self.channel = channel
        self.true_branch = true_branch


def get_current_os():
    # Translates platform.system() values to corresponding OmahaProxy OS names.
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
        except json.JSONDecodeError as e:
            fail(f'Failed to parse release metata response from '
                 f'{release_info_url}')


def get_release_metadata_by_version(release_info):
    uri = (f'https://omahaproxy.appspot.com/deps.json?version='
           f'{release_info.version}')
    json_response = fetch_json(uri)
    release_info.branch_position = json_response['chromium_base_position']
    release_info.true_branch = json_response['chromium_branch']


def get_release_metadata_by_channel(release_info):
    uri = (f'https://omahaproxy.appspot.com/all.json?'
           f'os={release_info.os}&channel={release_info.channel}')
    json_response = fetch_json(uri)[0]['versions'][0]
    release_info.branch_position = json_response['branch_base_position']
    release_info.true_branch = json_response['true_branch']
    release_info.version = json_response['version']


def get_release_metadata(release_info):
    # If the OS unspecified, detect and use the current OS.
    if not release_info.os:
        release_info.os = get_current_os()
    if release_info.branch_position:
        return
    elif release_info.version:
        get_release_metadata_by_version(release_info)
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
    def ReportHook(blocknum, blocksize, totalsize):
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

    # Translates OmahaProxy OS names to corresponding GCS storage paths.
    os_to_path = {
        'win64': 'win32-release_x64/asan-win32-release_x64',
        'linux': 'linux-release/asan-linux-release',
        'linux_debug': 'linux-debug/asan-linux-debug',
        'mac': 'mac-release/asan-mac-release',
        'mac_debug': 'mac-release/asan-mac-debug',
        # 'ios': 'ios-release/asan-ios-release', # unsupported
        'cros': 'linux-release-chromeos/asan-linux-release',
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
        outfile_path, _ = urlretrieve(asan_build_uri, outfile_path, ReportHook)
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
    group.add_argument('--version', help='Chrome version, e.g. 105.0.5191.2.')
    group.add_argument('--branch_position',
                       help='Chrome branch base position, e.g. 1025959.')
    group.add_argument('--channel',
                       choices=['canary', 'dev', 'beta', 'stable'],
                       help='Chromium channel, e.g. canary.')
    parser.add_argument('--os',
                        choices=['linux', 'mac', 'win64', 'cros'],
                        help='Operating system type as defined by OmahaProxy.')
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
                                 args.channel, None)
    main(release_info, args.download_directory, args.quiet)

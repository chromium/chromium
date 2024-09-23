#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import os
import pathlib
import urllib.request
import shlex
import shutil
import sys

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[2 if _THIS_DIR.name == '3pp' else 1]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import common


# I have arbitrarily chosen 100 as a number much more than the number of commits
# I expect to see in a day in R8.
_COMMITS_URL = 'https://r8.googlesource.com/r8/+log/HEAD~100..HEAD?format=JSON'
_ARCHIVE_URL = 'https://r8.googlesource.com/r8/+archive/{}.tar.gz'
_DEPOT_TOOLS_URL = ('https://chromium.googlesource.com/chromium/tools/'
                    'depot_tools/+archive/main.tar.gz')


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


def _install(version, output_prefix):
    temp_file_path = 'archive.tar.gz'
    common.download_file(_ARCHIVE_URL.format(version), temp_file_path)
    common.extract_tar(temp_file_path, '.')
    common.download_file(_DEPOT_TOOLS_URL, temp_file_path)
    common.extract_tar(temp_file_path, 'depot_tools')

    os.environ['PATH'] += os.path.pathsep + os.path.abspath('depot_tools')

    common.run_cmd(['tools/gradle.py', 'r8'])

    # Shrink (improves r8/d8 launch time):
    # Needs the -D flag to avoid compilation error, see http://b/311202383.
    # TODO(b/361625094): Use third_party/jdk/current after the Java
    # cross-reference pipeline has been updated to JDK 21.
    java_home = common.path_within_checkout('third_party/jdk11/current')
    common.run_cmd(
        shlex.split(f"""
        {java_home}/bin/java
            -Dcom.android.tools.r8.enableKeepAnnotations=1
            -cp build/libs/r8.jar
            com.android.tools.r8.R8
            --debug
            --classfile
            --no-minification
            --no-desugaring
            --pg-conf src/main/keep.txt
            --pg-conf {_THIS_DIR}/chromium_keeps.txt
            --lib {java_home}
            --output r8.jar
            build/libs/r8.jar
        """))
    out_dir = os.path.join(output_prefix, 'lib')
    os.makedirs(out_dir)
    shutil.move('r8.jar', out_dir)


def main():
    def do_latest():
        return get_commit_before_today()

    def do_install(args):
        _install(args.version, args.output_prefix)

    # TODO(b/361625094): Use third_party/jdk/current after the Java
    # cross-reference pipeline has been updated to JDK 21.
    common.main(do_latest=do_latest, do_install=do_install, runtime_deps=['//third_party/jdk11/current'])


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import os
import pathlib
import shutil
import sys
import urllib.request

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = next(p for p in _THIS_DIR.parents
                 if (p / 'build' / '3pp_common').is_dir())
sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))

import common
import graalvm

_COMMITS_URL = (
    'https://api.github.com/repos/google/turbine/commits?per_page=1&until={}')
_ARCHIVE_URL = 'https://github.com/google/turbine/archive/{}.tar.gz'


def _get_last_commit_of_the_week():
    """Returns data of last commit until the end of last week."""
    today = datetime.datetime.combine(datetime.date.today(), datetime.time.min)
    days_since_week_start = today.weekday()
    end_of_last_week = today - datetime.timedelta(days=days_since_week_start)
    url = _COMMITS_URL.format(end_of_last_week.isoformat())
    return json.load(urllib.request.urlopen(url))[0]


def do_latest():
    return _get_last_commit_of_the_week()['sha']


def do_install(args):
    archive_path = 'archive.tar.gz'
    common.download_file(_ARCHIVE_URL.format(args.version), archive_path)
    common.extract_tar(archive_path, 'turbine_src')
    src_dir = next(pathlib.Path('turbine_src').glob('turbine-*'))

    graal_home = graalvm.ensure_graalvm()
    env = os.environ.copy()
    env['JAVA_HOME'] = graal_home
    # Prevent mvn from sourcing /etc/mavenrc on Goobuntu workstations, which
    # forces Corp Airlock proxy settings and breaks local-test.
    env['MAVEN_SKIP_RC'] = '1'

    common.run_cmd(['mvn', '-v'], env=env)
    common.run_cmd(
        ['mvn', 'package', '-DskipTests=true', '-q', '-f', 'pom.xml'],
        cwd=str(src_dir),
        env=env,
    )

    built_jar = src_dir / 'target' / 'turbine-HEAD-SNAPSHOT-all-deps.jar'
    jar_path = os.path.join(args.output_prefix, 'turbine.jar')
    shutil.copy(str(built_jar), jar_path)

    sample_java = os.path.abspath('Sample.java')
    pathlib.Path(sample_java).write_text(
        'package foo;\npublic class Sample {}\n', encoding='utf-8')
    graalvm.build_native_image(
        jar_path,
        os.path.join(args.output_prefix, 'turbine'),
        main_class='com.google.turbine.main.Main',
        tracing_args_list=[[
            '--output',
            'sample_out.jar',
            '--sources',
            sample_java,
            '--javacopts',
            # Turbine needs --release or --bootclasspath to locate java.lang.
            '--release',
            '21',
            '--',
        ]],
    )


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
    )


if __name__ == '__main__':
    main()

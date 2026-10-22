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

    # Compiling turbine's own sources is used as a representative workload for
    # profile-guided optimizations. Its compile classpath includes
    # compile-only deps (e.g. AutoValue annotations).
    classpath_txt = os.path.abspath('turbine_classpath.txt')
    common.run_cmd(
        [
            'mvn', 'dependency:build-classpath', '-q', '-f', 'pom.xml',
            f'-Dmdep.outputFile={classpath_txt}'
        ],
        cwd=str(src_dir),
        env=env,
    )
    pgo_classpath = pathlib.Path(classpath_txt).read_text(
        encoding='utf-8').strip().split(os.pathsep)
    turbine_sources = sorted(
        str(p.resolve()) for d in ('java', 'target/generated-sources')
        for p in (src_dir / d).glob('**/*.java'))

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
        pgo_args_list=[
            ['--output', 'pgo_out.jar', '--classpath'] + pgo_classpath +
            ['--sources'] + turbine_sources +
            ['--javacopts', '--release', '21', '--'],
        ],
    )


def main():
    common.main(
        do_latest=do_latest,
        do_install=do_install,
        version_strings_func=graalvm.version_strings,
    )


if __name__ == '__main__':
    main()

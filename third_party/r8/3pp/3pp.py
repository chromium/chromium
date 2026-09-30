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
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[2 if _THIS_DIR.name == '3pp' else 1]

sys.path.insert(1, str(_SRC_ROOT / 'build' / '3pp_common'))
import common
import graalvm


# If false we roll to the latest branch, otherwise we roll to the latest commit.
_ROLL_TO_TIP_OF_TREE = False

# I have arbitrarily chosen 100 as a number much more than the number of commits
# I expect to see in a day in R8.
_COMMITS_URL = 'https://r8.googlesource.com/r8/+log/HEAD~100..HEAD?format=JSON'
_ARCHIVE_URL = 'https://r8.googlesource.com/r8/+archive/{}.tar.gz'
_DEPOT_TOOLS_URL = ('https://chromium.googlesource.com/chromium/tools/'
                    'depot_tools/+archive/main.tar.gz')
_BRANCHES_URL = 'https://r8-review.googlesource.com/projects/r8/branches?r=%5B1-9%5D.*'
_BRANCH_NAME_PREFIX = 'refs/heads/'

_CUSTOM_D8_SRC = 'third_party/r8/java/src/org/chromium/build/CustomD8.java'
# Path within the checkout. Cannot be under third_party/r8, since the 3pp recipe
# copies the package definition (this directory) to .3pp/chromium/third_party/r8
# after "checkout" runs, and fails if the directory already exists.
_CUSTOM_D8_CHECKOUT_SUBPATH = 'r8_custom_d8/CustomD8.java'
_CUSTOM_D8_MAIN_CLASS = 'org.chromium.build.CustomD8'
# Uses default methods, static interface methods, and lambdas so that tracing
# covers desugaring and desugar-dependency tracking.
_SAMPLE_IFACE_JAVA = """\
package org.chromium.sample;

public interface Iface {
  default String greet() { return "hi" + helper(); }
  static String helper() { return "!"; }
}
"""
_SAMPLE_JAVA = """\
package org.chromium.sample;

import java.util.function.Supplier;

public class Sample implements Iface {
  public Supplier<String> supplier() { return () -> greet(); }
}
"""

# Examples of version strings:
# 1.8, 2.5.10, 3.7-dev-aosp
def version_tuple(version):
    suffix = None
    if '-' in version:
        version, suffix = version.split('-', 1)
    version_segments = []
    for split in version.split('.'):
        try:
            version_segments.append(int(split))
        except:
            version_segments.append(split)
    if len(version_segments) < 3:
        version_segments += [0]*(3-len(version_segments))
    return (*version_segments, suffix)


def get_latest_dev_branch():
    response_string = urllib.request.urlopen(_BRANCHES_URL).read()
    # JSON response has a XSSI protection prefix )]}'
    parsed_response = json.loads(response_string.lstrip(b")]}'\n"))
    branch_revisions = {}
    for branch_info in parsed_response:
        # ref string looks like refs/heads/1.7
        branch_name = branch_info['ref'][len(_BRANCH_NAME_PREFIX):]
        branch_revisions[version_tuple(branch_name)] = branch_info['revision']
    latest_version = sorted(branch_revisions.keys())[-1]
    return branch_revisions[latest_version]


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
    java_home = graalvm.ensure_graalvm()
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

    _build_native_custom_d8(os.path.join(out_dir, 'r8.jar'),
                            os.path.join(output_prefix, 'bin', 'custom_d8'))


def _sample_d8_args(out_dir, jdk_home, classpath_jar, program_class_files,
                    tmp_prefix):
    """Returns argv lists covering the ways that dex.py invokes CustomD8."""
    os.makedirs(out_dir)
    intermediate_zip = os.path.join(out_dir, 'intermediate.zip')
    merged_dir = os.path.join(out_dir, 'merged')
    os.makedirs(merged_dir)
    common_args = ['--min-api', '21', '--lib', jdk_home]
    return [
        # Per-library dexing with desugaring.
        common_args + [
            '--intermediate',
            '--file-per-class-file',
            '--classpath',
            classpath_jar,
            '--desugar-dependencies',
            os.path.join(out_dir, 'desugar_deps.txt'),
            '--file-tmp-prefix',
            tmp_prefix,
            '--output',
            intermediate_zip,
        ] + program_class_files,
        # Dex merging.
        common_args + ['--release', '--output', merged_dir, intermediate_zip],
    ]


def _build_native_custom_d8(r8_jar, output_path):
    """Builds a GraalVM native-image of CustomD8 (as used by dex.py)."""
    graal_home = graalvm.ensure_graalvm()
    javac_bin = os.path.join(graal_home, 'bin', 'javac')

    custom_d8_classes = os.path.abspath('custom_d8_classes')
    common.run_cmd([
        javac_bin,
        '-cp',
        r8_jar,
        '-d',
        custom_d8_classes,
        common.path_within_checkout(_CUSTOM_D8_CHECKOUT_SUBPATH),
    ])

    # Inputs for the tracing agent, which records the reflection & resource
    # accesses that native-image needs to know about (e.g. R8's
    # reflectively-loaded ThreadingModule provider). Iface is passed via
    # --classpath so that desugaring records a dependency edge on it.
    sample_src = pathlib.Path('sample_src').resolve()
    sample_src.mkdir()
    (sample_src / 'Iface.java').write_text(_SAMPLE_IFACE_JAVA,
                                           encoding='utf-8')
    (sample_src / 'Sample.java').write_text(_SAMPLE_JAVA, encoding='utf-8')
    sample_classes = os.path.abspath('sample_classes')
    common.run_cmd([
        javac_bin, '--release', '11', '-d', sample_classes,
        str(sample_src / 'Iface.java'),
        str(sample_src / 'Sample.java')
    ])
    iface_class = 'org/chromium/sample/Iface.class'
    iface_jar = os.path.abspath('sample_classpath.jar')
    with zipfile.ZipFile(iface_jar, 'w') as z:
        z.write(os.path.join(sample_classes, iface_class), iface_class)
    program_class_files = sorted(
        str(p) for p in pathlib.Path(sample_classes).glob('**/*.class')
        if not p.name.startswith('Iface'))
    tmp_prefix = sample_classes + os.path.sep

    os.makedirs(os.path.dirname(output_path))
    graalvm.build_native_image(
        r8_jar,
        output_path,
        main_class=_CUSTOM_D8_MAIN_CLASS,
        extra_cp=[custom_d8_classes],
        tracing_args_list=_sample_d8_args(os.path.abspath('tracing_out'),
                                          graal_home, iface_jar,
                                          program_class_files, tmp_prefix),
        extra_native_image_args=[
            '-H:+UnlockExperimentalVMOptions',
            # R8 reads the JDK passed via --lib using the jrt:/ filesystem.
            '-H:+AllowJRTFileSystem',
        ],
    )

    # Tracing runs ignore failures, so check that the binary actually works
    # (e.g. that no reflection config is missing).
    smoke_out = os.path.abspath('smoke_test_out')
    for args in _sample_d8_args(smoke_out, graal_home, iface_jar,
                                program_class_files, tmp_prefix):
        common.run_cmd([output_path, f'-Djava.home={graal_home}'] + args)
    deps = pathlib.Path(smoke_out, 'desugar_deps.txt').read_text()
    if 'Iface.class' not in deps:
        raise RuntimeError(f'Unexpected desugar deps from custom_d8:\n{deps}')
    if not os.path.exists(os.path.join(smoke_out, 'merged', 'classes.dex')):
        raise RuntimeError('custom_d8 smoke test did not produce classes.dex')


def main():
    def do_latest():
        if _ROLL_TO_TIP_OF_TREE:
            return get_commit_before_today()
        return get_latest_dev_branch()

    def do_install(args):
        _install(args.version, args.output_prefix)

    common.main(do_latest=do_latest,
                do_install=do_install,
                runtime_deps=[('//' + _CUSTOM_D8_SRC,
                               _CUSTOM_D8_CHECKOUT_SUBPATH)])


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to run build benchmarks (e.g. incremental build time).

Example Command:
    tools/android/build_speed/benchmark.py all_incremental

Example Output:
    Summary
    gn args: target_os="android" use_goma=true android_fast_local_dev=true
    gn gen: 6.7s
    chrome_java_nosig: 36.1s avg (35.9s, 36.3s)
    chrome_java_sig: 38.9s avg (38.8s, 39.1s)
    chrome_java_res: 22.5s avg (22.5s, 22.4s)
    base_java_nosig: 41.0s avg (41.1s, 40.9s)
    base_java_sig: 93.1s avg (93.1s, 93.2s)

Note: This tool will make edits on files in your local repo. It will revert the
      edits afterwards.
"""

import argparse
import contextlib
import logging
import os
import pathlib
import re
import subprocess
import sys
import time
import shutil

USE_PYTHON_3 = f'This script will only run under python3.'

_SRC_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'build', 'android'))
from pylib import constants

# pylint: disable=line-too-long
_URL_BAR = 'chrome/android/java/src/org/chromium/chrome/browser/omnibox/UrlBar.java'

_COMMON_ARGS = [
    'target_os="android"',
    'use_goma=true',
]

_GN_ARG_PRESETS = {
    'fast_local_dev': _COMMON_ARGS + ['android_fast_local_dev=true'],
    'incremental_install': _COMMON_ARGS + ['incremental_install=true'],
}

_TARGETS = {
    'bundle': 'chrome_modern_public_bundle',
    'apk': 'chrome_public_apk',
}

_BENCHMARKS = {
    'all_incremental': {
        'suite': [
            'chrome_java_nosig',
            'chrome_java_sig',
            'chrome_java_res',
            'base_java_nosig',
            'base_java_sig',
        ],
    },
    'extra_incremental': {
        'suite': [
            'turbine_headers',
            'compile_java',
        ],
    },
    'chrome_java_nosig': {
        'kind': 'incremental',
        'from_string': '"Url',
        'to_string': '"Url1',
        'change_file': _URL_BAR,
    },
    'chrome_java_sig': {
        'kind': 'incremental',
        'from_string': 'UrlBar";',
        'to_string': 'UrlBar";public void NewInterfaceMethod(){}',
        'change_file': _URL_BAR,
    },
    'chrome_java_res': {
        'kind': 'incremental',
        'from_string': '14181C',
        'to_string': '14181D',
        'change_file': 'chrome/android/java/res/values/colors.xml',
    },
    'base_java_nosig': {
        'kind': 'incremental',
        'from_string': '"SysUtil',
        'to_string': '"SysUtil1',
        'change_file': 'base/android/java/src/org/chromium/base/SysUtils.java',
    },
    'base_java_sig': {
        'kind': 'incremental',
        'from_string': 'SysUtils";',
        'to_string': 'SysUtils";public void NewInterfaceMethod(){}',
        'change_file': 'base/android/java/src/org/chromium/base/SysUtils.java',
    },
    'turbine_headers': {
        'kind': 'incremental',
        'from_string': '# found in the LICENSE file.',
        'to_string': '#temporary_edit_for_benchmark.py',
        'change_file': 'build/android/gyp/turbine.py',
    },
    'compile_java': {
        'kind': 'incremental',
        'from_string': '# found in the LICENSE file.',
        'to_string': '#temporary_edit_for_benchmark.py',
        'change_file': 'build/android/gyp/compile_java.py',
    },
}


@contextlib.contextmanager
def _backup_file(file_path):
    file_backup_path = file_path + '.backup'
    logging.info('Creating %s for backup', file_backup_path)
    # Move the original file and copy back to preserve metadata.
    shutil.move(file_path, file_backup_path)
    try:
        shutil.copy(file_backup_path, file_path)
        yield
    finally:
        shutil.move(file_backup_path, file_path)


def _run_and_time_cmd(cmd):
    logging.debug('Running %s', cmd)
    start = time.time()
    try:
        # Since output can be verbose, only show it for debug/errors.
        show_output = logging.getLogger().isEnabledFor(logging.DEBUG)
        subprocess.run(cmd,
                       cwd=_SRC_ROOT,
                       capture_output=not show_output,
                       check=True,
                       text=True)
    except subprocess.CalledProcessError as e:
        logging.error('Output was: %s', e.output)
        raise
    return time.time() - start


def _run_gn_gen(out_dir):
    return _run_and_time_cmd(['gn', 'gen', '-C', out_dir])


def _run_autoninja(out_dir, *args):
    return _run_and_time_cmd(['autoninja', '-C', out_dir] + list(args))


def _run_incremental_benchmark(*, out_dir, target, from_string, to_string,
                               change_file):
    # This ensures that the only change is the one that this script makes.
    prep_time = _run_autoninja(out_dir, target)
    logging.info(f'Took {prep_time:.1f}s to prep this test')
    change_file_path = os.path.join(_SRC_ROOT, change_file)
    with _backup_file(change_file_path):
        with open(change_file_path, 'r') as f:
            content = f.read()
        with open(change_file_path, 'w') as f:
            new_content = re.sub(from_string, to_string, content)
            assert content != new_content, (
                f'Need to update {from_string} in {change_file}')
            f.write(new_content)
        yield _run_autoninja(out_dir, target)
    # Since we are restoring the original file, this is the same incremental
    # change, just reversed, so do a second run to save on prep time. This
    # ensures a minimum of two runs.
    pathlib.Path(change_file_path).touch()
    yield _run_autoninja(out_dir, target)


def _run_benchmark(*, kind, **kwargs):
    if kind == 'incremental':
        return _run_incremental_benchmark(**kwargs)
    else:
        raise NotImplementedError(f'Benchmark type {kind} is not defined.')


def _format_result(time_taken):
    avg_time = sum(time_taken) / len(time_taken)
    list_of_times = ', '.join(f'{t:.1f}s' for t in time_taken)
    result = f'{avg_time:.1f}s'
    if len(time_taken) > 1:
        result += f' avg ({list_of_times})'
    return result


def _parse_benchmarks(benchmarks):
    for benchmark in benchmarks:
        assert benchmark in _BENCHMARKS, (
            f'{benchmark} is not a valid benchmark/suite.')
        info = _BENCHMARKS[benchmark]
        if 'suite' in info:
            yield from _parse_benchmarks(info['suite'])
        else:
            yield benchmark, info


def run_benchmarks(benchmarks, gn_args, output_directory, target, repeat):
    out_dir = os.path.relpath(output_directory, _SRC_ROOT)
    args_gn_path = os.path.join(out_dir, 'args.gn')
    with _backup_file(args_gn_path):
        with open(args_gn_path, 'w') as f:
            # Use newlines instead of spaces since autoninja.py uses regex to
            # determine whether use_goma is turned on or off.
            f.write('\n'.join(gn_args))
        yield f'gn gen', [_run_gn_gen(out_dir)]
        for name, info in _parse_benchmarks(benchmarks):
            logging.info(f'Starting {name}...')
            time_taken = []
            for run_num in range(repeat):
                logging.info(f'Run number: {run_num + 1}')
                for elapsed in _run_benchmark(out_dir=out_dir,
                                              target=target,
                                              **info):
                    logging.info(f'Time: {elapsed:.1f}s')
                    time_taken.append(elapsed)
            logging.info(f'Completed {name}')
            logging.info('Result: %s', _format_result(time_taken))
            yield name, time_taken


def _list_benchmarks():
    strs = ['\nBenchmarks:']
    for name in _BENCHMARKS.keys():
        strs.append(f'    {name}')
    return '\n'.join(strs)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__ + _list_benchmarks(),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('benchmark',
                        nargs='+',
                        metavar='BENCHMARK',
                        choices=_BENCHMARKS.keys(),
                        help='Names of default benchmark(s) to run.')
    parser.add_argument('-A',
                        '--args',
                        choices=_GN_ARG_PRESETS.keys(),
                        default='fast_local_dev',
                        help='The set of GN args to use for these benchmarks.')
    parser.add_argument('--bundle',
                        action='store_true',
                        help='Switch the default target from apk to bundle.')
    parser.add_argument('-r',
                        '--repeat',
                        type=int,
                        default=1,
                        help='Number of times to repeat the benchmark.')
    parser.add_argument(
        '-C',
        '--output-directory',
        help='If outdir is not provided, will attempt to guess.')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='1 to print logging, 2 to print ninja output.')
    args = parser.parse_args()

    if args.output_directory:
        constants.SetOutputDirectory(args.output_directory)
    constants.CheckOutputDirectory()
    out_dir = constants.GetOutDirectory()

    if args.verbose >= 2:
        level = logging.DEBUG
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.WARNING
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if args.bundle:
        target = _TARGETS['bundle']
    else:
        target = _TARGETS['apk']

    gn_args = _GN_ARG_PRESETS[args.args]
    results = run_benchmarks(args.benchmark, gn_args, out_dir, target,
                             args.repeat)

    print('Summary')
    print(f'gn args: {" ".join(gn_args)}')
    print(f'target: {target}')
    for name, result in results:
        print(f'{name}: {_format_result(result)}')


if __name__ == '__main__':
    sys.exit(main())

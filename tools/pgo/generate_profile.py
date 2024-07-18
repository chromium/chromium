#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Runs benchmarks as described in docs/pgo.md, and similar to the PGO bots.

You need to build chrome with chrome_pgo_phase=1 (and the args.gn described
in docs/pgo.md for stage 1), and then run this like

    tools/pgo/generate_profile.py -C out/builddir

After that, the final profile will be in out/builddir/profile.profdata.
With that, you can do a second build with:

    is_official_build
    pgo_data_path = "//out/builddir/profile.profdata"

and with chrome_pgo_phase _not_ set. (It defaults to =2 in official builds.)
'''
# TODO(thakis): Make PGO bots actually run this script, crbug.com/1455237

import argparse
import glob
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from typing import Optional

_SRC_DIR = pathlib.Path(__file__).parents[2]
_TELEMETRY_DIR = _SRC_DIR / 'third_party/catapult/telemetry'
if str(_TELEMETRY_DIR) not in sys.path:
    sys.path.append(str(_TELEMETRY_DIR))
from telemetry.internal.backends import android_browser_backend_settings

ANDROID_SETTINGS = android_browser_backend_settings.ANDROID_BACKEND_SETTINGS

exe_ext = '.exe' if sys.platform == 'win32' else ''

THIS_DIR = os.path.dirname(__file__)
ROOT_DIR = f'{THIS_DIR}/../..'
UPDATE_PY = f'{THIS_DIR}/../clang/scripts/update.py'
LLVM_DIR = f'{ROOT_DIR}/third_party/llvm-build/Release+Asserts'
PROFDATA = f'{LLVM_DIR}/bin/llvm-profdata' + exe_ext


# This error is raised when LLVM failed to merge successfully.
class MergeError(RuntimeError):
    pass


# Use this custom Namespace to provide type checking and type hinting.
class OptionsNamespace(argparse.Namespace):
    builddir: str
    # Technically profiledir defaults to `None`, but it is always set before
    # parse_args returns, so leave it as `str` to avoid type errors for methods
    # that take an OptionsNamespace instance.
    profiledir: str
    keep_temps: bool
    android_browser: Optional[str]
    android_device_path: Optional[str]
    skip_profdata: bool
    run_public_benchmarks_only: bool
    temporal_trace_length: Optional[int]
    repeats: int
    verbose: int
    quiet: int


def parse_args():
    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    # ▼▼▼▼▼ Please update OptionsNamespace when adding or modifying args. ▼▼▼▼▼
    parser.add_argument('-C',
                        '--builddir',
                        help='Path to build directory.',
                        required=True)
    parser.add_argument('--profiledir',
                        help='Path to store temporary profiles, default is '
                        'builddir/profile.')
    parser.add_argument('--keep-temps',
                        action='store_true',
                        default=False,
                        help='Whether to keep temp files')
    parser.add_argument('--android-browser',
                        help='The type of android browser to test, e.g. '
                        'android-trichrome-bundle.')
    parser.add_argument('--android-device-path',
                        help='The device path to pull profiles from. By '
                        'default this is '
                        '/data/data/<package>/cache/pgo_profiles/ but you can '
                        'override it for your device if needed.')
    parser.add_argument('--skip-profdata',
                        action='store_true',
                        default=False,
                        help='Only run benchmarks and skip merging profile '
                        'data. Used for sample-based profiling for Propeller '
                        'and BOLT')
    parser.add_argument(
        '--run-public-benchmarks-only',
        action='store_true',
        default=False,
        help='Only run benchmarks that do not require any special access. See '
        'https://www.chromium.org/developers/telemetry/upload_to_cloud_storage/#request-access-for-google-partners '
        'for more information.')
    parser.add_argument(
        '--temporal-trace-length',
        type=int,
        help='Add flags necessary for temporal PGO (experimental).')
    parser.add_argument(
        '-r',
        '--repeats',
        type=int,
        default=5,
        help='Number of times to attempt each benchmark if it fails, default 5.'
    )
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='Increase verbosity level (repeat as needed)')
    parser.add_argument('-q',
                        '--quiet',
                        action='count',
                        default=0,
                        help='Decrease verbosity level (passed through to '
                        'run_benchmark.)')
    # ▲▲▲▲▲ Please update OptionsNamespace when adding or modifying args. ▲▲▲▲▲

    args = parser.parse_args(namespace=OptionsNamespace())

    # This is load-bearing:
    # - `open` (used by run_benchmark) needs absolute paths
    # - `open` sets the cwd to `/`, so LLVM_PROFILE_FILE must
    #   be an absolute path
    # Both are based off this variable.
    # See also https://crbug.com/1478279
    args.builddir = os.path.realpath(args.builddir)

    if args.android_browser and not args.android_device_path:
        for settings in ANDROID_SETTINGS:
            if settings.browser_type == args.android_browser:
                package = settings.package
                break
        else:
            assert False, f'Unable to find {args.android_browser} settings.'
        args.android_device_path = (f'/data/data/{package}/cache/pgo_profiles')

    if not args.profiledir:
        args.profiledir = f'{args.builddir}/profile'

    return args


def run_profdata_merge(output_path, input_files, args: OptionsNamespace):
    if args.temporal_trace_length:
        extra_args = [
            '--temporal-profile-max-trace-length',
            str(args.temporal_trace_length)
        ]
    else:
        extra_args = []
    proc = subprocess.run([PROFDATA, 'merge', '-o', output_path] + extra_args +
                          input_files,
                          check=True,
                          capture_output=True,
                          text=True)
    output = str(proc.stdout) + str(proc.stderr)
    print(output)
    if 'invalid profile created' in output:
        # This is necessary because for some reason this invalid data is kept
        # and only a warning is issued by llvm.
        raise MergeError('Failed to generate valid profile data.')


def run_benchmark(benchmark_args: list[str], args: OptionsNamespace):
    '''Puts profdata in {profiledir}/{args[0]}.profdata'''

    # Include the first 2 args since per-story benchmarks use [name, --story=s].
    name = '_'.join(benchmark_args[:2])

    # Clean up intermediate files from previous runs.
    profraw_path = f'{args.profiledir}/{name}/raw'
    if os.path.exists(profraw_path):
        shutil.rmtree(profraw_path)
    os.makedirs(profraw_path, exist_ok=True)
    profdata_path = f'{args.profiledir}/{name}.profdata'
    if os.path.exists(profdata_path):
        os.remove(profdata_path)

    env = os.environ.copy()
    if args.android_browser:
        env['CHROME_PGO_PROFILING'] = '1'
    else:
        env['LLVM_PROFILE_FILE'] = f'{profraw_path}/default-%2m.profraw'

    cmd = ['vpython3', 'tools/perf/run_benchmark'] + benchmark_args + [
        '--assert-gpu-compositing',
        # Abort immediately when any story fails, since a failed story fails
        # to produce valid profdata. Fail fast and rely on repeats to get a
        # valid profdata.
        '--max-failures=0',
    ] + ['-v'] * args.verbose + ['-q'] * args.quiet

    if args.android_browser:
        cmd += [
            f'--browser={args.android_browser}',
            '--fetch-device-data',
            '--fetch-device-data-platform=android',
            f'--fetch-data-path-device={args.android_device_path}',
            f'--fetch-data-path-local={profraw_path}',
        ]
    else:
        if sys.platform == 'darwin':
            exe_path = f'{args.builddir}/Chromium.app/Contents/MacOS/Chromium'
        else:
            exe_path = f'{args.builddir}/chrome' + exe_ext
        cmd += [
            '--browser=exact',
            f'--browser-executable={exe_path}',
        ]

    subprocess.run(cmd,
                   check=True,
                   shell=sys.platform == 'win32',
                   env=env,
                   cwd=ROOT_DIR)

    if args.skip_profdata:
        return

    # Android's `adb pull` does not allow * globbing (i.e. pulling
    # pgo_profiles/*). Since the naming of profraw files can vary, pull the
    # directory and check subdirectories recursively for .profraw files.
    profraw_files = glob.glob(f'{profraw_path}/**/*.profraw', recursive=True)
    if not profraw_files:
        raise RuntimeError(f'No profraw files found in {profraw_path}')

    run_profdata_merge(profdata_path, profraw_files, args)

    # Test merge to prevent issues like: https://crbug.com/353702041
    with tempfile.NamedTemporaryFile() as f:
        run_profdata_merge(f.name, [profdata_path], args)


def run_benchmark_with_repeats(benchmark_args: list[str],
                               args: OptionsNamespace):
    '''Runs the benchmark with provided args, return # of times it failed.'''
    assert args.repeats > 0, 'repeats must be at least 1'
    for idx in range(args.repeats):
        try:
            run_benchmark(benchmark_args, args)
            return idx
        except (subprocess.CalledProcessError, MergeError) as e:
            if isinstance(e, subprocess.CalledProcessError):
                if e.stdout:
                    print(e.stdout)
                if e.stderr:
                    print(e.stderr)
            if idx < args.repeats - 1:
                logging.warning('%s', e)
                logging.warning(
                    f'Retry attempt {idx + 1} for {benchmark_args}')
            else:
                logging.error(f'Failed {args.repeats} times')
                raise e
    # This statement can never be reached due to the above `raise e` but is here
    # to appease the typechecker.
    return args.repeats


def get_stories(benchmark_args: list[str], args: OptionsNamespace):
    print_stories_cmd = [
        'vpython3',
        'tools/perf/run_benchmark',
    ] + benchmark_args + [
        '--print-only=stories',
        '--print-only-runnable',  # This is essential to skip filtered stories.
        f'--browser={args.android_browser}',
    ]
    proc = subprocess.run(print_stories_cmd, text=True, capture_output=True)
    stories = []
    for line in proc.stdout.splitlines():
        if line and not line.startswith(('[', 'View results at')):
            stories.append(line)
    return stories


def run_benchmarks(benchmarks: list[list[str]], args: OptionsNamespace):
    fail_count = 0
    for benchmark_args in benchmarks:
        if not args.android_browser:
            fail_count += run_benchmark_with_repeats(benchmark_args, args)
        else:
            for story in get_stories(benchmark_args, args):
                per_story_args = [benchmark_args[0], f'--story={story}']
                fail_count += run_benchmark_with_repeats(per_story_args, args)
    return fail_count


def merge_profdata(args: OptionsNamespace):
    profile_output_path = f'{args.builddir}/profile.profdata'
    profdata_files = glob.glob(f'{args.profiledir}/*.profdata')
    if not profdata_files:
        raise RuntimeError(f'No profdata files found in {args.profiledir}')

    run_profdata_merge(profile_output_path, profdata_files, args)

    if args.temporal_trace_length:
        orderfile_cmd = [
            PROFDATA, 'order', profile_output_path, '-o',
            f'{args.builddir}/orderfile.txt'
        ]
        subprocess.run(orderfile_cmd, check=True)


def main():
    args = parse_args()

    if args.verbose >= 2:
        level = logging.DEBUG
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.WARNING
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if not os.path.exists(PROFDATA):
        logging.warning(f'{PROFDATA} does not exist, downloading it')
        subprocess.run([sys.executable, UPDATE_PY, '--package=coverage_tools'],
                       check=True)
    assert os.path.exists(PROFDATA), f'{PROFDATA} does not exist'

    if os.path.exists(args.profiledir):
        logging.warning('Removing %s', args.profiledir)
        shutil.rmtree(args.profiledir)

    # Run the shortest benchmarks first to fail early if anything is wrong.
    benchmarks: list[list[str]] = [
        ['speedometer3'],
        ['jetstream2'],
    ]

    # These benchmarks require special access permissions:
    # https://www.chromium.org/developers/telemetry/upload_to_cloud_storage/#request-access-for-google-partners
    if not args.run_public_benchmarks_only:
        platform = 'mobile' if args.android_browser else 'desktop'
        benchmarks.append([
            f'system_health.common_{platform}',
            '--run-abridged-story-set',
        ])
        benchmarks.append([
            f'rendering.{platform}',
            '--also-run-disabled-tests',
            '--story-tag-filter=motionmark_fixed_2_seconds',
            '--story-filter-exclude=motionmark_fixed_2_seconds_images',
        ])
        if sys.platform == 'darwin':
            benchmarks.append([
                f'rendering.{platform}',
                '--also-run-disabled-tests',
                '--story-tag-filter=motionmark_fixed_2_seconds',
                '--extra-browser-args=--enable-features=SkiaGraphite',
            ])

    fail_count = run_benchmarks(benchmarks, args)
    if fail_count:
        logging.warning(f'Of the {len(benchmarks)} benchmarks, there were '
                        f'{fail_count} failures that were resolved by repeat '
                        'runs.')

    if not args.skip_profdata:
        merge_profdata(args)

    if not args.keep_temps:
        logging.info('Cleaning up %s, use --keep-temps to keep it.',
                     args.profiledir)
        shutil.rmtree(args.profiledir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

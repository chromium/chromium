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
import os
import pathlib
import shutil
import subprocess
import sys

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


def parse_args():
    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-C',
                        '--builddir',
                        help='Path to build directory.',
                        required=True)
    parser.add_argument('--profiledir',
                        help='Path to store temporary profiles, default is '
                        'builddir/profile.')
    parser.add_argument('--keep-temps',
                        action='store_true',
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

    args = parser.parse_args()

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


def run_benchmark(benchmark_args: list[str], args):
    '''Puts profdata in {profiledir}/{args[0]}.profdata'''
    name = benchmark_args[0]

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
    ] + ['-v'] * args.verbose

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

    if not args.skip_profdata:
        # Android's `adb pull` does not allow * globbing (i.e. pulling
        # pgo_profiles/*). Since the naming of profraw files can vary, pull
        # the directory and check subdirectories recursively for .profraw
        # files.
        profraw_files = glob.glob(f'{profraw_path}/**/*.profraw',
                                  recursive=True)
        if not profraw_files:
            raise RuntimeError(f'No profraw files found in {profraw_path}')
        subprocess.run([PROFDATA, 'merge', '-o', profdata_path] +
                       profraw_files,
                       check=True)


def run_benchmark_with_repeats(benchmark_args: list[str], args):
    assert args.repeats > 0, 'repeats must be at least 1'
    for idx in range(args.repeats):
        try:
            print(f'Running attempt {idx} for {benchmark_args}')
            run_benchmark(benchmark_args, args)
            # Succeeded!
            return
        except subprocess.CalledProcessError as e:
            if idx < args.repeats - 1:
                print(e)
                print(f'Retrying... ')
            else:
                print(f'Failed {args.repeats} times')
                raise e


def run_benchmarks(benchmarks: list[list[str]], args):
    for benchmark_args in benchmarks:
        run_benchmark_with_repeats(benchmark_args, args)


def merge_profdata(args):
    merge_cmd = [PROFDATA, 'merge']
    if args.temporal_trace_length:
        merge_cmd += [
            '--temporal-profile-max-trace-length',
            str(args.temporal_trace_length)
        ]
    profile_output_path = f'{args.builddir}/profile.profdata'
    merge_cmd += ['-o', profile_output_path]
    profdata_files = glob.glob(f'{args.profiledir}/*.profdata')
    if not profdata_files:
        print(f'No profdata files found in {args.profiledir}')
    subprocess.run(merge_cmd + profdata_files, check=True)

    if args.temporal_trace_length:
        orderfile_cmd = [
            PROFDATA, 'order', profile_output_path, '-o',
            f'{args.builddir}/orderfile.txt'
        ]
        subprocess.run(orderfile_cmd, check=True)


def main():
    args = parse_args()

    if not os.path.exists(PROFDATA):
        print(f'{PROFDATA} does not exist, downloading it')
        subprocess.run([sys.executable, UPDATE_PY, '--package=coverage_tools'],
                       check=True)
    assert os.path.exists(PROFDATA), f'{PROFDATA} does not exist'

    if os.path.exists(args.profiledir):
        print(f'Removing {args.profiledir}')
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

    run_benchmarks(benchmarks, args)

    if not args.skip_profdata:
        merge_profdata(args)

    if not args.keep_temps:
        print(f'Cleaning up {args.profiledir}, use --keep-temps to keep it.')
        shutil.rmtree(args.profiledir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

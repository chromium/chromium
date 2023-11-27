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


def main():
    if not os.path.exists(PROFDATA):
        print(f'{PROFDATA} does not exist, downloading it')
        subprocess.run([sys.executable, UPDATE_PY, '--package=coverage_tools'],
                       check=True)

    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-C',
                        help='Path to build directory',
                        required=True,
                        metavar='builddir',
                        dest='builddir')
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
    builddir = os.path.realpath(args.builddir)
    if sys.platform == 'darwin':
        chrome_path = f'{builddir}/Chromium.app/Contents/MacOS/Chromium'
    elif args.android_browser:
        chrome_path = None
        if not args.android_device_path:
            for settings in ANDROID_SETTINGS:
                if settings.browser_type == args.android_browser:
                    package = settings.package
                    break
            else:
                assert False, f'Unable to find {args.android_browser} settings.'
            args.android_device_path = (
                f'/data/data/{package}/cache/pgo_profiles')
    else:
        chrome_path = f'{builddir}/chrome' + exe_ext
    profiledir = f'{builddir}/profile'

    def run_benchmark(benchmark_args):
        '''Puts profdata in {profiledir}/{args[0]}.profdata'''
        name = benchmark_args[0]
        profraw_path = f'{profiledir}/{name}/raw'
        if os.path.exists(profraw_path):
            shutil.rmtree(profraw_path)
        os.makedirs(profraw_path, exist_ok=True)

        env = os.environ.copy()
        if args.android_browser:
            env['CHROME_PGO_PROFILING'] = '1'
        else:
            env['LLVM_PROFILE_FILE'] = f'{profraw_path}/default-%2m.profraw'

        cmd = ['vpython3', 'tools/perf/run_benchmark'] + benchmark_args + [
            '--assert-gpu-compositing'
        ] + ['-v'] * args.verbose

        if args.android_browser:
            cmd += [
                f'--browser={args.android_browser}', '--fetch-device-data',
                '--fetch-device-data-platform=android',
                f'--fetch-data-path-device={args.android_device_path}',
                f'--fetch-data-path-local={profraw_path}'
            ]
        else:
            cmd += [
                '--browser=exact',
                f'--browser-executable={chrome_path}',
            ]

        subprocess.run(cmd, check=True, env=env, cwd=ROOT_DIR)
        profdata_path = f'{profiledir}/{name}.profdata'

        # Android's `adb pull` does not allow * globbing (i.e. pulling
        # pgo_profiles/*). Since the naming of profraw files can vary, pull the
        # directory and check subdirectories recursively for .profraw files.
        subprocess.run(
            [PROFDATA, 'merge', '-o', profdata_path] +
            glob.glob(f'{profraw_path}/**/*.profraw', recursive=True),
            check=True)

    if os.path.exists(profiledir):
        shutil.rmtree(profiledir)

    # Run the shortest benchmark first to fail early if anything is wrong.
    run_benchmark(['speedometer2'])
    if args.android_browser:
        run_benchmark(
            ['system_health.common_mobile', '--run-abridged-story-set'])
    else:
        run_benchmark(
            ['system_health.common_desktop', '--run-abridged-story-set'])
    run_benchmark(['jetstream2'])
    if args.android_browser:
        run_benchmark([
            'rendering.mobile', '--also-run-disabled-tests',
            '--story-tag-filter=motionmark_fixed_2_seconds'
        ])
    else:
        run_benchmark([
            'rendering.desktop', '--also-run-disabled-tests',
            '--story-tag-filter=motionmark_fixed_2_seconds'
        ])

    subprocess.run([PROFDATA, 'merge', '-o', f'{builddir}/profile.profdata'] +
                   glob.glob(f'{profiledir}/*.profdata'),
                   check=True)

    if not args.keep_temps:
        shutil.rmtree(profiledir)


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env vpython3
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

**IMPORTANT**: If you add any new deps for this script, make sure that it is
    also added to the template in //tools/pgo/BUILD.gn as data or data_deps, as
    this script is run as an isolated script on the bots, which means that only
    listed data and data_deps are available to it when run on the bot.
'''

import argparse
import glob
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from typing import List, Optional

_SRC_DIR = pathlib.Path(__file__).parents[2]
_TELEMETRY_DIR = _SRC_DIR / 'third_party/catapult/telemetry'
if str(_TELEMETRY_DIR) not in sys.path:
    sys.path.append(str(_TELEMETRY_DIR))
from telemetry.internal.backends import android_browser_backend_settings

_ANDROID_SETTINGS = android_browser_backend_settings.ANDROID_BACKEND_SETTINGS

# This is mutable, set to True on the first benchmark run and used to avoid
# re-installing the browser on subsequent benchmark runs.
_android_browser_installed = False

# See https://crbug.com/373822787 for how this value was calculated.
_ANDROID_64_BLOCK_COUNT_THRESHOLD = 2200000000

_EXE_EXT = '.exe' if sys.platform == 'win32' else ''
_THIS_DIR = os.path.dirname(__file__)
_ROOT_DIR = f'{_THIS_DIR}/../..'
_UPDATE_PY = f'{_THIS_DIR}/../clang/scripts/update.py'
_LLVM_DIR = f'{_ROOT_DIR}/third_party/llvm-build/Release+Asserts'
_PROFDATA = f'{_LLVM_DIR}/bin/llvm-profdata{_EXE_EXT}'

# This is necessary to get proper logging on bots and locally. If this script is
# run through run_isolated_script_test.py, a root logger would have already been
# set up. Thus for this script's logging to appear (and not disrupt other
# loggers) it needs to use its own logger.
_LOGGER = logging.getLogger(__name__)


# This error is raised when LLVM failed to merge successfully.
class MergeError(RuntimeError):
    pass


# Use this custom Namespace to provide type checking and type hinting.
class OptionsNamespace(argparse.Namespace):
    builddir: str
    # Technically profiledir and outputdir default to `None`, but they are
    # always set before parse_args returns, so leave it as `str` to avoid type
    # errors for methods that take an OptionsNamespace instance.
    outputdir: str
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
    # The following are bot-specific args.
    isolated_script_test_output: Optional[str]
    isolated_script_test_perf_output: Optional[str]
    # TODO(wnwen): Remove this when no longer needed.
    suffix: str


def parse_args():
    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    # ▼▼▼▼▼ Please update OptionsNamespace when adding or modifying args. ▼▼▼▼▼
    parser.add_argument('-C',
                        '--builddir',
                        help='Path to build directory.',
                        required=True)
    parser.add_argument(
        '--outputdir',
        help='Path to store final outputs, default is builddir.')
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
    parser.add_argument(
        '--android-device-path',
        help='The device path to pull profiles from. By '
        'default this is /data_mirror/data_ce/null/0/<package>'
        '/cache/pgo_profiles/ but you can override it for your '
        'device if needed.')
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
    parser.add_argument('--isolated-script-test-output',
                        help='Output.json file that the script can write to.')
    parser.add_argument('--isolated-script-test-perf-output',
                        help='Deprecated and ignored, but bots pass it.')
    # ▲▲▲▲▲ Please update OptionsNamespace when adding or modifying args. ▲▲▲▲▲

    args = parser.parse_args(namespace=OptionsNamespace())

    # This is load-bearing:
    # - `open` (used by run_benchmark) needs absolute paths
    # - `open` sets the cwd to `/`, so LLVM_PROFILE_FILE must
    #   be an absolute path
    # Both are based off this variable.
    # See also https://crbug.com/1478279
    args.builddir = os.path.realpath(args.builddir)
    _LOGGER.info(f"Build directory: {args.builddir}")

    if args.android_browser:
        _LOGGER.info(f"Android browser: {args.android_browser}")
        if not args.android_device_path:
            for settings in _ANDROID_SETTINGS:
                if settings.browser_type == args.android_browser:
                    package = settings.package
                    break
            else:
                raise ValueError(
                    f'Unable to find {args.android_browser} settings.')
            args.android_device_path = (
                f'/data_mirror/data_ce/null/0/{package}/cache/pgo_profiles')
            _LOGGER.info("Using default Android device path: "
                         f"{args.android_device_path}")
        else:
            _LOGGER.info("Using provided Android device path: "
                         f"{args.android_device_path}")

    if not args.profiledir:
        args.profiledir = f'{args.builddir}/profile'

    if not args.outputdir:
        args.outputdir = args.builddir

    if args.isolated_script_test_output:
        args.outputdir = os.path.dirname(args.isolated_script_test_output)
        args.suffix = '.profraw'
    else:
        args.suffix = '.profdata'

    _LOGGER.info(f"Output directory: {args.outputdir}")
    _LOGGER.info(f"Profile directory: {args.profiledir}")

    return args


def run_profdata_merge(output_path, input_files, args: OptionsNamespace):
    _LOGGER.info(
        f"Merging {len(input_files)} profile files into {output_path}")
    if args.temporal_trace_length:
        extra_args = [
            '--temporal-profile-max-trace-length',
            str(args.temporal_trace_length)
        ]
        _LOGGER.debug(
            f"Using temporal trace length: {args.temporal_trace_length}")

    else:
        extra_args = []
    filtered_input_files = []
    for f in input_files:
        size_in_bytes = os.path.getsize(f)
        if size_in_bytes <= 1 * 1024 * 1024:
            # A valid profile on android is usually 108MB, and typically 2-3 of
            # those are produced, so this would be less than 0.5% ignored. These
            # small ones need to be ignored since otherwise they fail the merge.
            _LOGGER.warning(f'Skipping due to size={size_in_bytes} <1MB: {f}')
        else:
            filtered_input_files.append(f)

    if not filtered_input_files:
        raise MergeError('No valid profraw/profdata file after filter.')

    cmd = [_PROFDATA, 'merge', '-o', output_path
           ] + extra_args + filtered_input_files
    _LOGGER.debug(f"Running command: {' '.join(cmd)}")

    proc = subprocess.run(cmd, check=True, capture_output=True, text=True)
    output = str(proc.stdout) + str(proc.stderr)
    _LOGGER.debug(f"llvm-profdata output:\n{output}")
    if 'invalid profile created' in output:
        # This is necessary because for some reason this invalid data is kept
        # and only a warning is issued by llvm.
        raise MergeError('Failed to generate valid profile data.')


def run_profdata_show(file_name, topn=1000):
    _LOGGER.info(f'Calculating topn={topn} for {file_name}')
    cmd = [_PROFDATA, 'show', '-topn', str(topn), file_name]
    _LOGGER.debug(f"Running command: {' '.join(cmd)}")
    proc = subprocess.run(cmd, check=True, capture_output=True, text=True)
    _LOGGER.debug(proc.stdout)
    return proc.stdout


def get_max_internal_block_count(file_name):
    for line in run_profdata_show(file_name).splitlines():
        if line.startswith("Maximum internal block count: "):
            return int(line.split(":", 1)[1])
    return None


def run_benchmark(benchmark_args: List[str], args: OptionsNamespace):
    '''Puts profdata in {profiledir}/{args[0]}.profdata'''
    global _android_browser_installed

    _LOGGER.info(f"Running benchmark: {' '.join(benchmark_args)}")

    # Include the first 2 args since per-story benchmarks use [name, --story=s].
    name = '_'.join(benchmark_args[:2])

    # Clean up intermediate files from previous runs.
    profraw_path = f'{args.profiledir}/{name}/raw'
    _LOGGER.debug(f"Raw profile path: {profraw_path}")

    if os.path.exists(profraw_path):
        _LOGGER.debug(
            f"Removing existing raw profile directory: {profraw_path}")
        shutil.rmtree(profraw_path)
    os.makedirs(profraw_path, exist_ok=True)

    profdata_path = f'{args.profiledir}/{name}{args.suffix}'
    _LOGGER.debug(f"profdata path: {profdata_path}")
    if os.path.exists(profdata_path):
        _LOGGER.debug(f"Removing existing profdata file: {profdata_path}")
        os.remove(profdata_path)

    env = os.environ.copy()
    if args.android_browser:
        env['CHROME_PGO_PROFILING'] = '1'
        _LOGGER.debug("Set environment variable CHROME_PGO_PROFILING=1")

    else:
        env['LLVM_PROFILE_FILE'] = f'{profraw_path}/default-%2m.profraw'
        _LOGGER.debug("Set environment variable "
                      f"LLVM_PROFILE_FILE={env['LLVM_PROFILE_FILE']}")

    cmd = ['vpython3', 'tools/perf/run_benchmark'] + benchmark_args + [
        f'--chromium-output-directory={args.builddir}',
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
        if _android_browser_installed:
            cmd += ['--assume-browser-already-installed']
        else:
            _android_browser_installed = True
        _LOGGER.debug(
            f"Running benchmark on Android with command: {' '.join(cmd)}")
    else:
        if sys.platform == 'darwin':
            exe_path = f'{args.builddir}/Chromium.app/Contents/MacOS/Chromium'
        else:
            exe_path = f'{args.builddir}/chrome' + _EXE_EXT
        cmd += [
            '--browser=exact',
            f'--browser-executable={exe_path}',
        ]

        _LOGGER.debug(
            f"Running benchmark locally with command: {' '.join(cmd)}")

    subprocess.run(cmd,
                   check=True,
                   shell=sys.platform == 'win32',
                   env=env,
                   cwd=_ROOT_DIR)

    if args.skip_profdata:
        _LOGGER.info("Skipping profdata merging")

        return

    # Android's `adb pull` does not allow * globbing (i.e. pulling
    # pgo_profiles/*). Since the naming of profraw files can vary, pull the
    # directory and check subdirectories recursively for .profraw files.
    profraw_files = glob.glob(f'{profraw_path}/**/*.profraw', recursive=True)
    _LOGGER.debug(f"Found {len(profraw_files)} profraw files")
    if not profraw_files:
        raise RuntimeError(f'No profraw files found in {profraw_path}')

    run_profdata_merge(profdata_path, profraw_files, args)

    # Test merge to prevent issues like: https://crbug.com/353702041
    with tempfile.NamedTemporaryFile() as f:
        _LOGGER.debug("Testing profdata merge")

        run_profdata_merge(f.name, [profdata_path], args)


def run_benchmark_with_repeats(benchmark_args: List[str],
                               args: OptionsNamespace):
    '''Runs the benchmark with provided args, return # of times it failed.'''
    assert args.repeats > 0, 'repeats must be at least 1'
    for idx in range(args.repeats):
        try:
            _LOGGER.info(f"Running benchmark attempt {idx + 1}/{args.repeats}")

            run_benchmark(benchmark_args, args)
            _LOGGER.info(f"Benchmark succeeded on attempt {idx+1}")

            return idx
        except (subprocess.CalledProcessError, MergeError) as e:
            if isinstance(e, subprocess.CalledProcessError):
                if e.stdout:
                    _LOGGER.error(f"Stdout:\n{e.stdout}")
                if e.stderr:
                    _LOGGER.error(f"Stderr:\n{e.stderr}")
            if idx < args.repeats - 1:
                _LOGGER.warning('%s', e)
                _LOGGER.warning(
                    f'Retry attempt {idx + 1} for {benchmark_args}')
            else:
                _LOGGER.error(f'Failed {args.repeats} times')
                raise e
    # This statement can never be reached due to the above `raise e` but is here
    # to appease the typechecker.
    return args.repeats


def get_stories(benchmark_args: List[str], args: OptionsNamespace):
    _LOGGER.info(f"Getting stories for benchmark: {' '.join(benchmark_args)}")
    print_stories_cmd = [
        'vpython3',
        'tools/perf/run_benchmark',
    ] + benchmark_args + [
        '--print-only=stories',
        '--print-only-runnable',  # This is essential to skip filtered stories.
        f'--browser={args.android_browser}',
    ]
    _LOGGER.debug(f"Running command: {' '.join(print_stories_cmd)}")

    # Avoid setting check=True here since the return code is 111 for success.
    proc = subprocess.run(print_stories_cmd,
                          text=True,
                          capture_output=True,
                          cwd=_ROOT_DIR)

    stories = []
    for line in proc.stdout.splitlines():
        if line and not line.startswith(('[', 'View results at')):
            stories.append(line)
    _LOGGER.debug(f"Found {len(stories)} stories")
    return stories


def run_benchmarks(benchmarks: List[List[str]], args: OptionsNamespace):
    fail_count = 0
    for benchmark_args in benchmarks:
        _LOGGER.info(f"Starting benchmark: {' '.join(benchmark_args)}")
        if not args.android_browser:
            fail_count += run_benchmark_with_repeats(benchmark_args, args)
        else:
            stories = get_stories(benchmark_args, args)
            for story in stories:
                _LOGGER.info(f"Running story: {story}")
                per_story_args = [benchmark_args[0], f'--story={story}']
                fail_count += run_benchmark_with_repeats(per_story_args, args)
    return fail_count


def merge_profdata(profile_output_path: str, args: OptionsNamespace):
    _LOGGER.info(f"Merging all profdata files into: {profile_output_path}")
    profdata_files = glob.glob(f'{args.profiledir}/*{args.suffix}')
    _LOGGER.debug(f"Found {len(profdata_files)} profdata files")
    if not profdata_files:
        raise RuntimeError(f'No profdata files found in {args.profiledir}')

    run_profdata_merge(profile_output_path, profdata_files, args)

    if args.temporal_trace_length:
        _LOGGER.info("Generating orderfile for temporal PGO")
        orderfile_cmd = [
            _PROFDATA, 'order', profile_output_path, '-o',
            f'{args.outputdir}/orderfile.txt'
        ]
        _LOGGER.debug(f"Running command: {' '.join(orderfile_cmd)}")

        subprocess.run(orderfile_cmd, check=True)


def main():
    args = parse_args()

    handler = logging.StreamHandler()
    formatter = logging.Formatter(
        '%(levelname).1s %(relativeCreated)6d %(message)s')
    handler.setFormatter(formatter)
    _LOGGER.addHandler(handler)

    if args.verbose >= 2:
        _LOGGER.setLevel(logging.DEBUG)
    elif args.verbose == 1:
        _LOGGER.setLevel(logging.INFO)
    else:
        _LOGGER.setLevel(logging.WARN)

    if not os.path.exists(_PROFDATA):
        if args.isolated_script_test_output:
            _LOGGER.warning(f'{_PROFDATA} missing on bot, {_LLVM_DIR}:')
            for root, _, files in os.walk(_LLVM_DIR):
                for f in files:
                    _LOGGER.warning(f'> {os.path.join(root, f)}')
        else:
            _LOGGER.warning(f'{_PROFDATA} does not exist, downloading it')
            subprocess.run(
                [sys.executable, _UPDATE_PY, '--package=coverage_tools'],
                check=True)
    assert os.path.exists(_PROFDATA), f'{_PROFDATA} does not exist'

    if os.path.exists(args.profiledir):
        _LOGGER.warning('Removing %s', args.profiledir)
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
        _LOGGER.warning(f'Of the {len(benchmarks)} benchmarks, there were '
                        f'{fail_count} failures that were resolved by repeat '
                        'runs.')

    if not args.skip_profdata:
        profile_output_path = f'{args.outputdir}/profile{args.suffix}'
        merge_profdata(profile_output_path, args)
        if ('64' in str(args.android_browser)
                and args.isolated_script_test_output):
            # We are on the arm64 bot, where we want to avoid perf regressions.
            max_internal_block_count = get_max_internal_block_count(
                profile_output_path)
            _LOGGER.info(
                f'Maximum internal block count: {max_internal_block_count}')
            assert max_internal_block_count is not None, 'Failed to get ' \
                'max internal block count from profdata.'
            # TODO(crbug.com/373822787): Remove this when no longer needed.
            if max_internal_block_count < _ANDROID_64_BLOCK_COUNT_THRESHOLD:
                raise MergeError(
                    f"max_internal_block_count={max_internal_block_count} is "
                    f"lower than {_ANDROID_64_BLOCK_COUNT_THRESHOLD}.")

    if not args.keep_temps:
        _LOGGER.info('Cleaning up %s, use --keep-temps to keep it.',
                     args.profiledir)
        shutil.rmtree(args.profiledir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

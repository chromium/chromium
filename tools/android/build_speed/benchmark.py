#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to run build benchmarks (e.g. incremental build time).

Example Command:
    tools/android/build_speed/benchmark.py all_incremental

Example Output:
    Summary
    gn args: target_os="android" use_remoteexec=true incremental_install=true
    gn_gen: 6.7s
    chrome_nosig: 36.1s avg (35.9s, 36.3s)
    chrome_sig: 38.9s avg (38.8s, 39.1s)
    base_nosig: 41.0s avg (41.1s, 40.9s)
    base_sig: 93.1s avg (93.1s, 93.2s)

Note: This tool will make edits on files in your local repo. It will revert the
      edits afterwards.
"""

import argparse
import collections
import contextlib
import dataclasses
import functools
import json
import logging
import os
import pathlib
import random
import re
import signal
import statistics
import subprocess
import sys
import time
import shutil

from typing import Dict, Callable, Iterator, List, Optional, Tuple

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(1, str(_SRC_ROOT / 'build'))
import gn_helpers

sys.path.insert(1, str(_SRC_ROOT / 'build/android'))
from pylib import constants
import devil_chromium

sys.path.insert(1, str(_SRC_ROOT / 'third_party/catapult/devil'))
from devil.android.sdk import adb_wrapper
from devil.android import device_utils

_GN_PATH = _SRC_ROOT / 'third_party/depot_tools/gn.py'

_EMULATOR_AVD_DIR = _SRC_ROOT / 'tools/android/avd'
_AVD_SCRIPT = _EMULATOR_AVD_DIR / 'avd.py'
_AVD_CONFIG_DIR = _EMULATOR_AVD_DIR / 'proto'
_SECONDS_TO_POLL_FOR_EMULATOR = 30

# Anything not in this list is assumed to be x64.
_X86_EMULATORS = {
    'generic_android23.textpb': 'x86',
    'generic_android24.textpb': 'x86',
    'generic_android25.textpb': 'x86',
    'generic_android26.textpb': 'x86',
    'generic_android26_local.textpb': 'x86',
    'generic_android27.textpb': 'x86',
    'generic_android27_local.textpb': 'x86',
    'android_28_google_apis_x86.textpb': 'x86',
    'android_28_google_apis_x86_local.textpb': 'x86',
    'android_29_google_apis_x86.textpb': 'x86',
    'android_29_google_apis_x86_local.textpb': 'x86',
    'android_30_google_apis_x86.textpb': 'x86',
    'android_30_google_apis_x86_local.textpb': 'x86',
}

_GN_ARGS = [
    'target_os="android"',
    'use_remoteexec=true',
]

_NO_SERVER = [
    'android_static_analysis="on"',
]

_SERVER = [
    'android_static_analysis="build_server"',
]

_INCREMENTAL_INSTALL = [
    'incremental_install=true',
]

_NO_COMPONENT_BUILD = [
    'is_component_build=false',
]

_TARGETS = {
    'bundle': 'monochrome_public_bundle',
    'apk': 'chrome_public_apk',
    'test': 'chrome_public_test_apk',
}

_SUITES = {
    'all_incremental': [
        'chrome_nosig',
        'chrome_sig',
        'module_public_sig',
        'module_internal_nosig',
        'base_nosig',
        'base_sig',
        'cta_test_sig',
    ],
    'all_chrome_java': [
        'chrome_nosig',
        'chrome_sig',
    ],
    'all_module_java': [
        'module_public_sig',
        'module_internal_nosig',
    ],
    'all_base_java': [
        'base_nosig',
        'base_sig',
    ],
    'extra_incremental': [
        'turbine_headers',
        'compile_java',
        'errorprone',
        'write_build_config',
    ],
}


@dataclasses.dataclass
class Benchmark:
    name: str
    from_string: str = ''
    to_string: str = ''
    change_file: str = ''

    # Both of these require an emulator to be present.
    can_install: bool = False
    can_run: bool = False

    # Useful for large test apks to not run every test in it.
    test_filter: str = ''


_BENCHMARKS = [
    Benchmark(
        name='chrome_nosig',
        from_string='IntentHandler";',
        to_string='Different<sub>UniqueString";',
        change_file=
        'chrome/android/java/src/org/chromium/chrome/browser/IntentHandler.java',  # pylint: disable=line-too-long
        can_install=True,
    ),
    Benchmark(
        name='chrome_sig',
        from_string='public ChromeApplicationImpl() {}',
        to_string=
        'public ChromeApplicationImpl() {};public void NewInterface<sub>Method(){}',  # pylint: disable=line-too-long
        change_file=
        'chrome/android/java/src/org/chromium/chrome/browser/ChromeApplicationImpl.java',  # pylint: disable=line-too-long
        can_install=True,
    ),
    Benchmark(
        name='module_public_sig',
        from_string='INVALID_WINDOW_ID = -1',
        to_string='INVALID_WINDOW_ID = -<sub>',
        change_file=
        'chrome/browser/tabwindow/android/java/src/org/chromium/chrome/browser/tabwindow/TabWindowManager.java',  # pylint: disable=line-too-long
        can_install=True,
    ),
    Benchmark(
        name='module_internal_nosig',
        from_string='"TabModelSelector',
        to_string='"DifferentUnique<sub>String',
        change_file=
        'chrome/browser/tabwindow/internal/android/java/src/org/chromium/chrome/browser/tabwindow/TabWindowManagerImpl.java',  # pylint: disable=line-too-long
        can_install=True,
    ),
    Benchmark(
        name='base_nosig',
        from_string='"PathUtil',
        to_string='"PathUtil<sub>1',
        change_file='base/android/java/src/org/chromium/base/PathUtils.java',
        can_install=True,
    ),
    Benchmark(
        name='base_sig',
        from_string='PathUtils";',
        to_string='PathUtils";public void NewInterface<sub>Method(){}',
        change_file='base/android/java/src/org/chromium/base/PathUtils.java',
        can_install=True,
    ),
    Benchmark(
        name='turbine_headers',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark<sub>.py',
        change_file='build/android/gyp/turbine.py',
    ),
    Benchmark(
        name='compile_java',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark<sub>.py',
        change_file='build/android/gyp/compile_java.py',
    ),
    Benchmark(
        name='errorprone',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark<sub>.py',
        change_file='build/android/gyp/errorprone.py',
    ),
    Benchmark(
        name='write_build_config',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark<sub>.py',
        change_file='build/android/gyp/write_build_config.py',
    ),
    Benchmark(
        name='cta_test_sig',
        from_string='public void testStartOnBlankPage() {',
        to_string=
        'public void NewInterface<sub>Method(){};public void testStartOnBlankPage() {',  # pylint: disable=line-too-long
        change_file=
        'chrome/android/javatests/src/org/chromium/chrome/browser/ExampleFreshCtaTest.java',  # pylint: disable=line-too-long
        can_run=True,
        test_filter='*ExampleFreshCtaTest*',
    ),
]

_BENCHMARK_FROM_NAME = {benchmark.name: benchmark for benchmark in _BENCHMARKS}


@contextlib.contextmanager
def _backup_file(file_path: pathlib.Path):
    if not file_path.exists():
        try:
            yield
        finally:
            if file_path.exists():
                file_path.unlink()
        return
    file_backup_path = file_path.with_suffix('.backup')
    logging.info('Creating %s for backup', file_backup_path)
    # Move the original file and copy back to preserve metadata.
    shutil.move(file_path, file_backup_path)
    try:
        shutil.copy(file_backup_path, file_path)
        yield
    finally:
        shutil.move(file_backup_path, file_path)
        # Update the timestamp so ninja knows to rebuild next time.
        pathlib.Path(file_path).touch()


def _detect_emulators() -> List[device_utils.DeviceUtils]:
    return [
        device_utils.DeviceUtils(d) for d in adb_wrapper.AdbWrapper.Devices()
        if isinstance(d, adb_wrapper.AdbWrapper) and d.is_emulator
    ]


def _poll_for_emulators(
        condition: Callable[[List[device_utils.DeviceUtils]], bool], *,
        expected: str):
    for sec in range(_SECONDS_TO_POLL_FOR_EMULATOR):
        emulators = _detect_emulators()
        if condition(emulators):
            break
        logging.debug(f'Waited {sec}s for emulator to become ready...')
        time.sleep(1)
    else:
        raise Exception(
            f'Emulator is not ready after {_SECONDS_TO_POLL_FOR_EMULATOR}s. '
            f'Expected {expected}.')


@contextlib.contextmanager
def _emulator(emulator_avd_name):
    logging.info(f'Starting emulator image: {emulator_avd_name}')
    _poll_for_emulators(lambda emulators: len(emulators) == 0,
                        expected='no running emulators')
    avd_config = _AVD_CONFIG_DIR / emulator_avd_name
    is_verbose = logging.getLogger().isEnabledFor(logging.INFO)
    cmd = [_AVD_SCRIPT, 'start', '--avd-config', avd_config]
    if not is_verbose:
        cmd.append('-q')
    logging.debug('Running AVD cmd: %s', cmd)
    try:
        # Ensure that stdout goes to stderr so that timing output does not get
        # mixed with logging output.
        subprocess.run(cmd, check=True, stdout=sys.stderr)
    except subprocess.CalledProcessError:
        logging.error(f'Unable to start the emulator {emulator_avd_name}')
        raise
    _poll_for_emulators(lambda emulators: len(emulators) == 1,
                        expected='exactly one emulator started successfully')
    device = _detect_emulators()[0]
    assert device.adb is not None
    try:
        # Ensure the emulator and its disk are fully set up.
        device.WaitUntilFullyBooted(decrypt=True)
        logging.info('Started emulator: %s', device.serial)
        yield device
    finally:
        device.adb.Emu('kill')
        _poll_for_emulators(lambda emulators: len(emulators) == 0,
                            expected='no running emulators')
        logging.info('Stopped emulator.')


def _run_and_time_cmd(cmd: List[str], *, dry_run: bool) -> float:
    logging.debug('Running %s', cmd)
    if dry_run:
        logging.warning('Dry run, skipping and returning random time.')
        return random.uniform(1.0, 10.0)
    start = time.time()
    try:
        # Since output can be verbose, only show it for debug/errors.
        show_output = logging.getLogger().isEnabledFor(logging.DEBUG)
        # Set autoninja stdout to /dev/null so that autoninja does not set this
        # to an anonymous pipe.
        env = os.environ.copy()
        env['AUTONINJA_STDOUT_NAME'] = '/dev/null'
        # Ensure that stdout goes to stderr so that timing output does not get
        # mixed with logging output.
        subprocess.run(cmd,
                       cwd=_SRC_ROOT,
                       capture_output=not show_output,
                       stdout=sys.stderr if show_output else None,
                       check=True,
                       text=True,
                       env=env)
    except subprocess.CalledProcessError as e:
        logging.error('Output was: %s', e.output)
        raise
    return time.time() - start


def _run_gn_gen(out_dir: pathlib.Path, *, dry_run: bool) -> float:
    return _run_and_time_cmd(
        [sys.executable,
         str(_GN_PATH), 'gen', '-C',
         str(out_dir)],
        dry_run=dry_run)


def _terminate_build_server_if_needed(out_dir: pathlib.Path):
    cmd = ["pgrep", "-f", "fast_local_dev_server.py"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if not proc.stdout:
        logging.info('No build server detected via pgrep.')
        return
    pid = proc.stdout.strip()
    logging.info(f'Detected build server with pid {pid}, sending SIGINT...')
    os.kill(int(pid), signal.SIGINT)
    for _ in range(5):
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if not proc.stdout:
            logging.info('Successfully terminated build server.')
            break
        logging.info('Waiting 1s for build server to terminate...')
        time.sleep(1)
    else:
        raise Exception('Build server still running after waiting 5s.')


def _compile(out_dir: pathlib.Path, target: str, *, dry_run: bool) -> float:
    cmd = gn_helpers.CreateBuildCommand(str(out_dir))
    try:
        return _run_and_time_cmd(cmd + [target], dry_run=dry_run)
    finally:
        # This ensures that the build server does not affect subsequent runs.
        _terminate_build_server_if_needed(out_dir)


def _run_install(out_dir: pathlib.Path, target: str, device_serial: str, *,
                 dry_run: bool) -> float:
    # Example script path: out/Debug/bin/chrome_public_apk
    script_path = out_dir / 'bin' / target
    # Disable first run to get a more accurate timing of startup.
    cmd = [
        str(script_path), 'run', '--device', device_serial,
        '--args=--disable-fre', '--exit-on-match',
        '^Successfully loaded native library$'
    ]
    if logging.getLogger().isEnabledFor(logging.DEBUG):
        cmd += ['-vv']
    return _run_and_time_cmd(cmd, dry_run=dry_run)


def _run_test(out_dir: pathlib.Path, target: str, device_serial: str,
              test_filter: str, *, dry_run: bool) -> float:
    # Example script path: out/Debug/bin/run_chrome_public_test_apk
    script_path = out_dir / 'bin' / f'run_{target}'
    cmd = [str(script_path), '--fast-local-dev', '--device', device_serial]
    if test_filter:
        cmd += ['-f', test_filter]
    if logging.getLogger().isEnabledFor(logging.DEBUG):
        cmd += ['-vv']
    return _run_and_time_cmd(cmd, dry_run=dry_run)


def _execute_benchmark_stages(benchmark: Benchmark, out_dir: pathlib.Path,
                              target: str,
                              emulator: Optional[device_utils.DeviceUtils], *,
                              dry_run: bool) -> List[Tuple[str, float]]:
    if benchmark.can_install or benchmark.can_run:
        assert emulator, f'An emulator is required for {benchmark}'
    results = [(f'{benchmark.name}_compile',
                _compile(out_dir, target, dry_run=dry_run))]
    if benchmark.can_install:
        results.append((f'{benchmark.name}_install',
                        _run_install(out_dir,
                                     target,
                                     emulator.serial,
                                     dry_run=dry_run)))
    if benchmark.can_run:
        results.append((f'{benchmark.name}_run',
                        _run_test(out_dir,
                                  target,
                                  emulator.serial,
                                  benchmark.test_filter,
                                  dry_run=dry_run)))
    return results


def _run_benchmark(benchmark: Benchmark, out_dir: pathlib.Path, target: str,
                   emulator: Optional[device_utils.DeviceUtils], *,
                   dry_run: bool) -> List[Tuple[str, float]]:
    # This ensures that the only change is the one that this script makes.
    logging.info('Prepping benchmark...')
    results = _execute_benchmark_stages(benchmark,
                                        out_dir,
                                        target,
                                        emulator,
                                        dry_run=dry_run)
    for name, elapsed in results:
        logging.info(f'Took {elapsed:.1f}s to prep {name}.')
    logging.info('Starting actual test...')
    change_file_path = _SRC_ROOT / benchmark.change_file
    with _backup_file(change_file_path):
        with open(change_file_path, 'r') as f:
            content = f.read()
        with open(change_file_path, 'w') as f:
            # 2 billion is less than 2^31-1, which is the maximum positive int
            # in java and less than the maximum negative int, which is -2^31.
            replacement = benchmark.to_string.replace(
                '<sub>', str(random.randint(1, 2_000_000_000)))
            logging.info(
                f'Replacing {benchmark.from_string} with {replacement}')
            new_content = content.replace(benchmark.from_string, replacement)
            assert content != new_content, (
                f'Need to update {benchmark.from_string} in '
                f'{benchmark.change_file}')
            f.write(new_content)
        return _execute_benchmark_stages(benchmark,
                                         out_dir,
                                         target,
                                         emulator,
                                         dry_run=dry_run)


def _format_result(time_taken: List[float]) -> str:
    avg_time = sum(time_taken) / len(time_taken)
    result = f'{avg_time:.1f}s'
    if len(time_taken) > 1:
        standard_deviation = statistics.stdev(time_taken)
        list_of_times = ', '.join(f'{t:.1f}s' for t in time_taken)
        result += f' avg [sd: {standard_deviation:.1f}s] ({list_of_times})'
    return result


def _parse_benchmarks(benchmarks: List[str]) -> Iterator[Benchmark]:
    for name in benchmarks:
        if name in _SUITES:
            for benchmark_name in _SUITES[name]:
                yield _BENCHMARK_FROM_NAME[benchmark_name]
        else:
            yield _BENCHMARK_FROM_NAME[name]


def run_benchmarks(benchmarks: List[str], gn_args: List[str],
                   output_directory: pathlib.Path, target: str, repeat: int,
                   emulator_avd_name: Optional[str], *, dry_run: bool) -> Dict:
    args_gn_path = output_directory / 'args.gn'
    if emulator_avd_name is None:
        emulator_ctx = contextlib.nullcontext
    else:
        emulator_ctx = functools.partial(_emulator, emulator_avd_name)
    timings = collections.defaultdict(list)
    with _backup_file(args_gn_path):
        with open(args_gn_path, 'w') as f:
            # Use newlines instead of spaces since autoninja.py uses regex to
            # determine whether use_remoteexec is turned on or off.
            f.write('\n'.join(gn_args))
        for run_num in range(repeat):
            logging.info(f'Run number: {run_num + 1}')
            timings['gn_gen'].append(
                _run_gn_gen(output_directory, dry_run=dry_run))
            for benchmark in _parse_benchmarks(benchmarks):
                logging.info(f'Starting {benchmark.name}...')
                # Start a fresh emulator for each benchmark to produce more
                # consistent results.
                with emulator_ctx() as emulator:
                    results = _run_benchmark(benchmark=benchmark,
                                             out_dir=output_directory,
                                             target=target,
                                             emulator=emulator,
                                             dry_run=dry_run)
                for name, elapsed in results:
                    logging.info(f'Completed {name}: {elapsed:.1f}s')
                    timings[name].append(elapsed)
    return timings


def _all_benchmark_and_suite_names() -> Iterator[str]:
    for key in _SUITES.keys():
        yield key
    for benchmark in _BENCHMARKS:
        yield benchmark.name


def _list_benchmarks() -> str:
    strs = ['\nSuites and Individual Benchmarks:']
    for name in _all_benchmark_and_suite_names():
        strs.append(f'    {name}')
    return '\n'.join(strs)


def main():
    assert __doc__ is not None
    parser = argparse.ArgumentParser(
        description=__doc__ + _list_benchmarks(),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        'benchmark',
        nargs='*',
        metavar='BENCHMARK',
        # Allow empty to just test `gn_gen` speed.
        choices=list(_all_benchmark_and_suite_names()) + [[]],
        help='Names of benchmark(s) or suites(s) to run.')
    parser.add_argument('--bundle',
                        action='store_true',
                        help='Switch the default target from apk to bundle.')
    parser.add_argument('--test',
                        action='store_true',
                        help='Switch the default target to a test apk.')
    parser.add_argument('--no-server',
                        action='store_true',
                        help='Do not start a faster local dev server before '
                        'running the test.')
    parser.add_argument('--no-incremental-install',
                        action='store_true',
                        help='Do not use incremental install.')
    parser.add_argument('--no-component-build',
                        action='store_true',
                        help='Turn off component build.')
    parser.add_argument('--build-64bit',
                        action='store_true',
                        help='Build 64-bit by default, even with no emulator.')
    parser.add_argument('-r',
                        '--repeat',
                        type=int,
                        default=1,
                        help='Number of times to repeat the benchmark.')
    parser.add_argument(
        '-C',
        '--output-directory',
        help='If outdir is not provided, will attempt to guess.')
    parser.add_argument('--emulator',
                        help='Specify this to override the default emulator.')
    parser.add_argument('--target',
                        help='Specify this to override the default target.')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='1 to print logging, 2 to print ninja output.')
    parser.add_argument('-q',
                        '--quiet',
                        action='store_true',
                        help='Do not print the summary.')
    parser.add_argument('--json',
                        action='store_true',
                        help='Output machine-readable output per benchmark.')
    parser.add_argument('-n',
                        '--dry-run',
                        action='store_true',
                        help='Do everything except the build/test/run '
                        'steps, which will return random times.')
    args = parser.parse_args()

    if args.output_directory:
        constants.SetOutputDirectory(args.output_directory)
    constants.CheckOutputDirectory()
    out_dir = pathlib.Path(constants.GetOutDirectory()).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.verbose >= 2:
        level = logging.DEBUG
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.WARNING
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    gn_args = _GN_ARGS.copy()
    if args.no_server:
        gn_args += _NO_SERVER
    else:
        gn_args += _SERVER
    if not args.no_incremental_install:
        gn_args += _INCREMENTAL_INSTALL
    if args.no_component_build:
        gn_args += _NO_COMPONENT_BUILD

    if args.emulator:
        devil_chromium.Initialize()
        logging.info('Using emulator %s', args.emulator)
        if args.emulator in _X86_EMULATORS:
            target_cpu = "x86"
        else:
            target_cpu = "x64"
    elif args.build_64bit:
        # Default to an emulator target_cpu when just building to be comparable
        # to building and installing on an emulator. It is likely that devs are
        # mostly using emulator builds so this is more valuable to track.
        target_cpu = "x64"
    else:
        target_cpu = "x86"
    gn_args.append(f'target_cpu="{target_cpu}"')

    if args.target:
        target = args.target
    elif args.bundle:
        target = _TARGETS['bundle']
    elif args.test:
        target = _TARGETS['test']
    else:
        target = _TARGETS['apk']

    results = run_benchmarks(args.benchmark,
                             gn_args,
                             out_dir,
                             target,
                             args.repeat,
                             args.emulator,
                             dry_run=args.dry_run)

    if args.json:
        json_results = []
        for name, timings in results.items():
            json_results.append({
                'name': name,
                'timings': timings,
                'emulator': args.emulator,
                'gn_args': gn_args,
                'target': target,
            })
        print(json.dumps(json_results, indent=2))
    elif not args.quiet:
        print(f'Summary')
        print(f'emulator: {args.emulator}')
        print(f'gn args: {" ".join(gn_args)}')
        print(f'target: {target}')
        for name, timings in results.items():
            print(f'{name}: {_format_result(timings)}')


if __name__ == '__main__':
    sys.exit(main())

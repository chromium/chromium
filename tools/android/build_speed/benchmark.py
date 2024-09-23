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
    gn gen: 6.7s
    chrome_java_nosig: 36.1s avg (35.9s, 36.3s)
    chrome_java_sig: 38.9s avg (38.8s, 39.1s)
    base_java_nosig: 41.0s avg (41.1s, 40.9s)
    base_java_sig: 93.1s avg (93.1s, 93.2s)

Note: This tool will make edits on files in your local repo. It will revert the
      edits afterwards.
"""

import argparse
import collections
import contextlib
import dataclasses
import functools
import logging
import pathlib
import re
import statistics
import subprocess
import sys
import time
import shutil

from typing import Dict, Callable, Iterator, List, Tuple, Optional

USE_PYTHON_3 = f'{__file__} will only run under python3.'

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(1, str(_SRC_ROOT / 'build'))
import gn_helpers

sys.path.insert(1, str(_SRC_ROOT / 'build/android'))
from pylib import constants
import devil_chromium

sys.path.insert(1, str(_SRC_ROOT / 'third_party/catapult/devil'))
from devil.android.sdk import adb_wrapper
from devil.android import device_utils

_GN_PATH = _SRC_ROOT / 'third_party/depot_tools/gn'

_EMULATOR_AVD_DIR = _SRC_ROOT / 'tools/android/avd'
_AVD_SCRIPT = _EMULATOR_AVD_DIR / 'avd.py'
_AVD_CONFIG_DIR = _EMULATOR_AVD_DIR / 'proto'
_SECONDS_TO_POLL_FOR_EMULATOR = 30

_SUPPORTED_EMULATORS = {
    'generic_android23.textpb': 'x86',
    'generic_android24.textpb': 'x86',
    'generic_android25.textpb': 'x86',
    'generic_android26.textpb': 'x86',
    'generic_android27.textpb': 'x86',
    'android_28_google_apis_x86.textpb': 'x86',
    'android_29_google_apis_x86.textpb': 'x86',
    'android_30_google_apis_x86.textpb': 'x86',
    'android_31_google_apis_x64.textpb': 'x64',
    'android_32_google_apis_x64_foldable.textpb': 'x64',
    'android_33_google_apis_x64': 'x64',
    'android_34_google_apis_x64': 'x64',
    'android_35_google_apis_x64': 'x64',
}

_GN_ARGS = [
    'target_os="android"',
    'incremental_install=true',
    'use_remoteexec=true',
]

_TARGETS = {
    'bundle': 'monochrome_public_bundle',
    'apk': 'chrome_public_apk',
}

_SUITES = {
    'all_incremental': [
        'chrome_java_nosig',
        'chrome_java_sig',
        'module_java_public_sig',
        'module_java_internal_nosig',
        'base_java_nosig',
        'base_java_sig',
    ],
    'all_chrome_java': [
        'chrome_java_nosig',
        'chrome_java_sig',
    ],
    'all_module_java': [
        'module_java_public_sig',
        'module_java_internal_nosig',
    ],
    'all_base_java': [
        'base_java_nosig',
        'base_java_sig',
    ],
    'extra_incremental': [
        'turbine_headers',
        'compile_java',
        'write_build_config',
    ],
}


@dataclasses.dataclass
class Benchmark:
    name: str
    is_incremental: bool = True
    can_build: bool = True
    can_install: bool = True
    from_string: str = ''
    to_string: str = ''
    change_file: str = ''


_BENCHMARKS = [
    Benchmark(
        name='chrome_java_nosig',
        from_string='super.onCreate();',
        to_string='super.onCreate();String test = "Test";',
        change_file=
        'chrome/android/java/src/org/chromium/chrome/browser/ChromeApplicationImpl.java',  # pylint: disable=line-too-long
    ),
    Benchmark(
        name='chrome_java_sig',
        from_string='private static final Object sLock = new Object();',
        to_string=
        'private static final Object sLock = new Object();public void NewInterfaceMethod(){}',
        change_file=
        'chrome/android/java/src/org/chromium/chrome/browser/ChromeApplicationImpl.java',  # pylint: disable=line-too-long
    ),
    Benchmark(
        name='module_java_public_sig',
        from_string='INVALID_WINDOW_INDEX = -1',
        to_string='INVALID_WINDOW_INDEX = -2',
        change_file=
        'chrome/browser/tabmodel/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManager.java',  # pylint: disable=line-too-long
    ),
    Benchmark(
        name='module_java_internal_nosig',
        from_string='"TabModelSelector',
        to_string='"DifferentUniqueString',
        change_file=
        'chrome/browser/tabmodel/internal/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManagerImpl.java',  # pylint: disable=line-too-long
    ),
    Benchmark(
        name='base_java_nosig',
        from_string='"SysUtil',
        to_string='"SysUtil1',
        change_file='base/android/java/src/org/chromium/base/SysUtils.java',
    ),
    Benchmark(
        name='base_java_sig',
        from_string='SysUtils";',
        to_string='SysUtils";public void NewInterfaceMethod(){}',
        change_file='base/android/java/src/org/chromium/base/SysUtils.java',
    ),
    Benchmark(
        name='turbine_headers',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark.py',
        change_file='build/android/gyp/turbine.py',
        can_install=False,
    ),
    Benchmark(
        name='compile_java',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark.py',
        change_file='build/android/gyp/compile_java.py',
        can_install=False,
    ),
    Benchmark(
        name='write_build_config',
        from_string='# found in the LICENSE file.',
        to_string='#temporary_edit_for_benchmark.py',
        change_file='build/android/gyp/write_build_config.py',
        can_install=False,
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


@contextlib.contextmanager
def _server():
    cmd = [_SRC_ROOT / 'build/android/fast_local_dev_server.py']
    # Avoid the build server's output polluting benchmark results, but allow
    # stderr to get through in case the build server fails with an error.
    # TODO(wnwen): Switch to using subprocess.run and check=True to quit if the
    #     server cannot be started.
    server_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL)
    logging.debug('Started fast local dev server.')
    try:
        yield
    finally:
        # Since Popen's default context manager just waits on exit, we need to
        # use our custom context manager to actually terminate the build server
        # when the current build is done to avoid skewing the next benchmark.
        server_proc.terminate()
        server_proc.wait()


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
    # Always start with --wipe-data to get consistent results. It adds around
    # 20 seconds to startup timing but is essential to avoid Timeout errors.
    # Set disk size to 16GB since the default 8GB is insufficient. Turns out
    # 32GB takes too long to startup (370 seconds).
    cmd = [
        _AVD_SCRIPT, 'start', '--wipe-data', '--avd-config', avd_config,
        '--disk-size', '16000'
    ]
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


def _run_and_time_cmd(cmd: List[str]) -> float:
    logging.debug('Running %s', cmd)
    start = time.time()
    try:
        # Since output can be verbose, only show it for debug/errors.
        show_output = logging.getLogger().isEnabledFor(logging.DEBUG)
        # Ensure that stdout goes to stderr so that timing output does not get
        # mixed with logging output.
        subprocess.run(cmd,
                       cwd=_SRC_ROOT,
                       capture_output=not show_output,
                       stdout=sys.stderr if show_output else None,
                       check=True,
                       text=True)
    except subprocess.CalledProcessError as e:
        logging.error('Output was: %s', e.output)
        raise
    return time.time() - start


def _run_gn_gen(out_dir: pathlib.Path) -> float:
    return _run_and_time_cmd([str(_GN_PATH), 'gen', '-C', str(out_dir)])


def _compile(out_dir: pathlib.Path, target: str, j: Optional[str]) -> float:
    cmd = gn_helpers.CreateBuildCommand(str(out_dir))
    if j is not None:
        cmd += ['-j', j]
    return _run_and_time_cmd(cmd + [target])


def _run_install(out_dir: pathlib.Path, target: str,
                 device_serial: str) -> float:
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
    return _run_and_time_cmd(cmd)


def _run_and_maybe_install(out_dir: pathlib.Path, target: str,
                           emulator: Optional[device_utils.DeviceUtils],
                           j: Optional[str]) -> float:
    total_time = _compile(out_dir, target, j)
    if emulator:
        total_time += _run_install(out_dir, target, emulator.serial)
    return total_time


def _run_benchmark(benchmark: Benchmark, out_dir: pathlib.Path, target: str,
                   emulator: Optional[device_utils.DeviceUtils],
                   j: Optional[str]) -> float:
    # This ensures that the only change is the one that this script makes.
    logging.info(f'Prepping benchmark...')
    if not benchmark.can_install:
        emulator = None
    prep_time = _run_and_maybe_install(out_dir, target, emulator, j)
    logging.info(f'Took {prep_time:.1f}s to prep.')
    logging.info(f'Starting actual test...')
    change_file_path = _SRC_ROOT / benchmark.change_file
    with _backup_file(change_file_path):
        with open(change_file_path, 'r') as f:
            content = f.read()
        with open(change_file_path, 'w') as f:
            new_content = re.sub(benchmark.from_string, benchmark.to_string,
                                 content)
            assert content != new_content, (
                f'Need to update {benchmark.from_string} in '
                f'{benchmark.change_file}')
            f.write(new_content)
        return _run_and_maybe_install(out_dir, target, emulator, j)


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
                   no_server: bool, emulator_avd_name: Optional[str],
                   j: Optional[str]) -> Dict[str, List[float]]:
    args_gn_path = output_directory / 'args.gn'
    if emulator_avd_name is None:
        emulator_ctx = contextlib.nullcontext
    else:
        emulator_ctx = functools.partial(_emulator, emulator_avd_name)
    server_ctx = _server if not no_server else contextlib.nullcontext
    timings = collections.defaultdict(list)
    with _backup_file(args_gn_path):
        with open(args_gn_path, 'w') as f:
            # Use newlines instead of spaces since autoninja.py uses regex to
            # determine whether use_remoteexec is turned on or off.
            f.write('\n'.join(gn_args))
        for run_num in range(repeat):
            logging.info(f'Run number: {run_num + 1}')
            timings['gn gen'].append(_run_gn_gen(output_directory))
            for benchmark in _parse_benchmarks(benchmarks):
                logging.info(f'Starting {benchmark.name}...')
                # Run the fast local dev server fresh for each benchmark run
                # to avoid later benchmarks being slower due to the server
                # accumulating queued tasks. Start a fresh emulator for each
                # benchmark to produce more consistent results.
                with emulator_ctx() as emulator, server_ctx():
                    elapsed = _run_benchmark(benchmark=benchmark,
                                             out_dir=output_directory,
                                             target=target,
                                             emulator=emulator,
                                             j=j)
                logging.info(f'Completed {benchmark.name}: {elapsed:.1f}s')
                timings[benchmark.name].append(elapsed)
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
        # Allow empty to just test `gn gen` speed.
        choices=list(_all_benchmark_and_suite_names()) + [[]],
        help='Names of benchmark(s) or suites(s) to run.')
    parser.add_argument('--bundle',
                        action='store_true',
                        help='Switch the default target from apk to bundle.')
    parser.add_argument('--no-server',
                        action='store_true',
                        help='Do not start a faster local dev server before '
                        'running the test.')
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
                        choices=list(_SUPPORTED_EMULATORS.keys()),
                        help='Specify this to override the default emulator.')
    parser.add_argument('--target',
                        help='Specify this to override the default target.')
    parser.add_argument('-j',
                        help='Pass -j to use ninja instead of autoninja.')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='1 to print logging, 2 to print ninja output.')
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

    gn_args = _GN_ARGS

    if args.emulator:
        devil_chromium.Initialize()
        logging.info('Using emulator %s', args.emulator)
        gn_args.append(f'target_cpu="{_SUPPORTED_EMULATORS[args.emulator]}"')
    else:
        # Default to an emulator target_cpu when just building to be comparable
        # to building and installing on an emulator. It is likely that devs are
        # mostly using emulator builds so this is more valuable to track.
        gn_args.append('target_cpu="x86"')

    if args.target:
        target = args.target
    else:
        target = _TARGETS['bundle' if args.bundle else 'apk']

    results = run_benchmarks(args.benchmark, gn_args, out_dir, target,
                             args.repeat, args.no_server, args.emulator,
                             args.j)

    server_str = f'{"not " if args.no_server else ""}using build server'
    print(f'Summary ({server_str})')
    print(f'emulator: {args.emulator}')
    print(f'gn args: {" ".join(gn_args)}')
    print(f'target: {target}')
    for name, timings in results.items():
        print(f'{name}: {_format_result(timings)}')


if __name__ == '__main__':
    sys.exit(main())

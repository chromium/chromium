#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to run build benchmarks (e.g. incremental build time).

Example Command:
    tools/android/build_speed/benchmark.py all_incremental

Example Output:
    Summary
    gn args: target_os="android" use_goma=true incremental_install=true
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
import collections
import contextlib
import dataclasses
import functools
import logging
import os
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
sys.path.append(str(_SRC_ROOT / 'build' / 'android'))
from pylib import constants
import devil_chromium

sys.path.append(str(_SRC_ROOT / 'third_party' / 'catapult' / 'devil'))
from devil.android.sdk import adb_wrapper
from devil.android import device_utils

_EMULATOR_AVD_DIR = _SRC_ROOT / 'tools' / 'android' / 'avd'
_AVD_SCRIPT = _EMULATOR_AVD_DIR / 'avd.py'
_AVD_CONFIG_DIR = _EMULATOR_AVD_DIR / 'proto'
_SECONDS_TO_POLL_FOR_EMULATOR = 30

_SUPPORTED_EMULATORS = {
    'generic_android23.textpb': 'x86',
    'generic_android24.textpb': 'x86',
    'generic_android25.textpb': 'x86',
    'generic_android27.textpb': 'x86',
    'generic_android28.textpb': 'x86',
    'generic_android29.textpb': 'x86',
    'generic_android30.textpb': 'x86',
    'generic_android31.textpb': 'x64',
}

_GN_ARGS = [
    'target_os="android"',
    'use_goma=true',
    'incremental_install=true',
]

_TARGETS = {
    'bundle': 'monochrome_public_bundle',
    'apk': 'chrome_public_apk',
}

_SUITES = {
    'all_incremental': [
        'chrome_java_nosig',
        'chrome_java_sig',
        'chrome_java_res',
        'module_java_public_sig',
        'module_java_internal_nosig',
        'base_java_nosig',
        'base_java_sig',
    ],
    'all_chrome_java': [
        'chrome_java_nosig',
        'chrome_java_sig',
        'chrome_java_res',
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
    info: Dict[str, str]


_BENCHMARKS = [
    Benchmark(
        'chrome_java_nosig',
        {
            'kind':
            'incremental_build_and_install',
            'from_string':
            'sInstance = instance;',
            'to_string':
            'sInstance = instance;String test = "Test";',
            # pylint: disable=line-too-long
            'change_file':
            'chrome/android/java/src/org/chromium/chrome/browser/AppHooks.java',
        }),
    Benchmark(
        'chrome_java_sig',
        {
            'kind':
            'incremental_build_and_install',
            'from_string':
            'AppHooksImpl sInstance;',
            'to_string':
            'AppHooksImpl sInstance;public void NewInterfaceMethod(){}',
            # pylint: disable=line-too-long
            'change_file':
            'chrome/android/java/src/org/chromium/chrome/browser/AppHooks.java',
        }),
    Benchmark(
        'chrome_java_res', {
            'kind': 'incremental_build_and_install',
            'from_string': '14181C',
            'to_string': '14181D',
            'change_file': 'chrome/android/java/res/values/colors.xml',
        }),
    Benchmark(
        'module_java_public_sig',
        {
            'kind':
            'incremental_build_and_install',
            'from_string':
            'INVALID_WINDOW_INDEX = -1',
            'to_string':
            'INVALID_WINDOW_INDEX = -2',
            # pylint: disable=line-too-long
            'change_file':
            'chrome/browser/tabmodel/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManager.java',
        }),
    Benchmark(
        'module_java_internal_nosig',
        {
            'kind':
            'incremental_build_and_install',
            'from_string':
            '"TabModelSelector',
            'to_string':
            '"DifferentUniqueString',
            # pylint: disable=line-too-long
            'change_file':
            'chrome/browser/tabmodel/internal/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManagerImpl.java',
        }),
    Benchmark(
        'base_java_nosig', {
            'kind': 'incremental_build_and_install',
            'from_string': '"SysUtil',
            'to_string': '"SysUtil1',
            'change_file':
            'base/android/java/src/org/chromium/base/SysUtils.java',
        }),
    Benchmark(
        'base_java_sig', {
            'kind': 'incremental_build_and_install',
            'from_string': 'SysUtils";',
            'to_string': 'SysUtils";public void NewInterfaceMethod(){}',
            'change_file':
            'base/android/java/src/org/chromium/base/SysUtils.java',
        }),
    Benchmark(
        'turbine_headers', {
            'kind': 'incremental_build',
            'from_string': '# found in the LICENSE file.',
            'to_string': '#temporary_edit_for_benchmark.py',
            'change_file': 'build/android/gyp/turbine.py',
        }),
    Benchmark(
        'compile_java', {
            'kind': 'incremental_build',
            'from_string': '# found in the LICENSE file.',
            'to_string': '#temporary_edit_for_benchmark.py',
            'change_file': 'build/android/gyp/compile_java.py',
        }),
    Benchmark(
        'write_build_config', {
            'kind': 'incremental_build',
            'from_string': '# found in the LICENSE file.',
            'to_string': '#temporary_edit_for_benchmark.py',
            'change_file': 'build/android/gyp/write_build_config.py',
        }),
]


@contextlib.contextmanager
def _backup_file(file_path: str):
    file_backup_path = file_path + '.backup'
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
    cmd = [_SRC_ROOT / 'build' / 'android' / 'fast_local_dev_server.py']
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
        if d.is_emulator
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
    cmd = [_AVD_SCRIPT, 'start', '--wipe-data', '--avd-config', avd_config]
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
    try:
        # Ensure the emulator and its disk are fully set up.
        device.WaitUntilFullyBooted(decrypt=True)
        if device.build_version_sdk >= 28:
            # In P, there are two settings:
            #  * hidden_api_policy_p_apps
            #  * hidden_api_policy_pre_p_apps
            # In Q, there is just one:
            #  * hidden_api_policy
            if device.build_version_sdk == 28:
                setting_name = 'hidden_api_policy_p_apps'
            else:
                setting_name = 'hidden_api_policy'
            device.adb.Shell(f'settings put global {setting_name} 0')
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


def _run_gn_gen(out_dir: str) -> float:
    return _run_and_time_cmd(['gn', 'gen', '-C', out_dir])


def _run_autoninja(out_dir: str, target: str) -> float:
    return _run_and_time_cmd(['autoninja', '-C', out_dir, target])


def _run_install(out_dir: str, target: str, device_serial: str) -> float:
    # Example script path: out/Debug/bin/chrome_public_apk
    script_path = os.path.join(out_dir, 'bin', target)
    # Disable first run to get a more accurate timing of startup.
    cmd = [
        script_path, 'run', '--device', device_serial, '--args=--disable-fre',
        '--exit-on-match', '^Successfully loaded native library$'
    ]
    if logging.getLogger().isEnabledFor(logging.DEBUG):
        cmd += ['-vv']
    return _run_and_time_cmd(cmd)


def _run_and_maybe_install(out_dir: str, target: str,
                           emulator: Optional[device_utils.DeviceUtils]
                           ) -> float:
    total_time = _run_autoninja(out_dir, target)
    if emulator:
        total_time += _run_install(out_dir, target, emulator.serial)
    return total_time


def _run_incremental_benchmark(*, out_dir: str, target: str, from_string: str,
                               to_string: str, change_file: str,
                               emulator: Optional[device_utils.DeviceUtils]
                               ) -> Iterator[float]:
    # This ensures that the only change is the one that this script makes.
    logging.info(f'Prepping incremental benchmark...')
    prep_time = _run_and_maybe_install(out_dir, target, emulator)
    logging.info(f'Took {prep_time:.1f}s to prep. Sleeping for 1 minute.')
    # 60s is enough to sufficiently reduce load and improve consistency. 30s
    # did not sufficiently lower standard deviation and 90s did not further
    # reduce standard deviation compared to 60s.
    time.sleep(60)
    logging.info(f'Starting actual test...')
    change_file_path = os.path.join(_SRC_ROOT, change_file)
    with _backup_file(change_file_path):
        with open(change_file_path, 'r') as f:
            content = f.read()
        with open(change_file_path, 'w') as f:
            new_content = re.sub(from_string, to_string, content)
            assert content != new_content, (
                f'Need to update {from_string} in {change_file}')
            f.write(new_content)
        return _run_and_maybe_install(out_dir, target, emulator)


def _run_benchmark(*, kind: str, emulator: Optional[device_utils.DeviceUtils],
                   **kwargs: Dict) -> Iterator[float]:
    if kind == 'incremental_build':
        assert not emulator, f'Install not supported for {kwargs}.'
        return _run_incremental_benchmark(emulator=None, **kwargs)
    elif kind == 'incremental_build_and_install':
        return _run_incremental_benchmark(emulator=emulator, **kwargs)
    else:
        raise NotImplementedError(f'Benchmark type {kind} is not defined.')


def _format_result(time_taken: List[float]) -> str:
    avg_time = sum(time_taken) / len(time_taken)
    result = f'{avg_time:.1f}s'
    if len(time_taken) > 1:
        standard_deviation = statistics.stdev(time_taken)
        list_of_times = ', '.join(f'{t:.1f}s' for t in time_taken)
        result += f' avg [sd: {standard_deviation:.1f}s] ({list_of_times})'
    return result


def _get_benchmark_for_name(name: str) -> Benchmark:
    for benchmark in _BENCHMARKS:
        if benchmark.name == name:
            return benchmark
    assert False, f'{name} is not a valid name.'


def _parse_benchmarks(benchmarks: List[str]) -> Iterator[Benchmark]:
    for name in benchmarks:
        if name in _SUITES:
            yield from _parse_benchmarks(_SUITES[name])
        else:
            yield _get_benchmark_for_name(name)


def run_benchmarks(benchmarks: List[str], gn_args: List[str],
                   output_directory: str, target: str, repeat: int,
                   no_server: bool,
                   emulator_avd_name: Optional[str]) -> Dict[str, List[float]]:
    out_dir = os.path.relpath(output_directory, _SRC_ROOT)
    args_gn_path = os.path.join(out_dir, 'args.gn')
    if emulator_avd_name is None:
        emulator_ctx = contextlib.nullcontext
    else:
        emulator_ctx = functools.partial(_emulator, emulator_avd_name)
    server_ctx = _server if not no_server else contextlib.nullcontext
    timings = collections.defaultdict(list)
    with _backup_file(args_gn_path):
        with open(args_gn_path, 'w') as f:
            # Use newlines instead of spaces since autoninja.py uses regex to
            # determine whether use_goma is turned on or off.
            f.write('\n'.join(gn_args))
        for run_num in range(repeat):
            logging.info(f'Run number: {run_num + 1}')
            timings['gn gen'].append(_run_gn_gen(out_dir))
            for benchmark in _parse_benchmarks(benchmarks):
                logging.info(f'Starting {benchmark.name}...')
                # Run the fast local dev server fresh for each benchmark run
                # to avoid later benchmarks being slower due to the server
                # accumulating queued tasks. Start a fresh emulator for each
                # benchmark to produce more consistent results.
                with emulator_ctx() as emulator, server_ctx():
                    elapsed = _run_benchmark(out_dir=out_dir,
                                             target=target,
                                             emulator=emulator,
                                             **benchmark.info)
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
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='1 to print logging, 2 to print ninja output.')
    args = parser.parse_args()

    if args.output_directory:
        constants.SetOutputDirectory(args.output_directory)
    constants.CheckOutputDirectory()
    out_dir: str = constants.GetOutDirectory()

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
                             args.repeat, args.no_server, args.emulator)
    server_str = f'{"not " if args.no_server else ""}using build server'
    print(f'Summary ({server_str})')
    print(f'emulator: {args.emulator}')
    print(f'gn args: {" ".join(gn_args)}')
    print(f'target: {target}')
    for name, timings in results.items():
        print(f'{name}: {_format_result(timings)}')


if __name__ == '__main__':
    sys.exit(main())

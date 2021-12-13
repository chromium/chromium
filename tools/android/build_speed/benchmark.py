#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
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
import contextlib
import dataclasses
import logging
import os
import pathlib
import re
import subprocess
import sys
import time
import shutil

from typing import Dict, Iterator, List, Tuple

USE_PYTHON_3 = f'{__file__} will only run under python3.'

_SRC_ROOT = pathlib.Path(__file__).parents[3].resolve()
sys.path.append(str(_SRC_ROOT / 'build' / 'android'))
from pylib import constants
import devil_chromium

sys.path.append(str(_SRC_ROOT / 'third_party' / 'catapult' / 'devil'))
from devil.android.sdk import adb_wrapper
from devil.android import device_utils

_EMULATOR_AVD_DIR = _SRC_ROOT / 'tools' / 'android' / 'avd'
_AVD_SCRIPT = _EMULATOR_AVD_DIR / 'avd.py'
# Use API 28 as it's the highest API version that monochrome supports.
_AVD_CONFIG = _EMULATOR_AVD_DIR / 'proto' / 'generic_android28.textpb'
_SECONDS_TO_WAIT_FOR_EMULATOR_STARTUP = 30

_GN_ARGS = [
    'target_os="android"',
    'use_goma=true',
    'incremental_install=true',
]

_EMULATOR_GN_ARGS = [
    'target_cpu="x86"',
]

_TARGETS = {
    'bundle': 'chrome_modern_public_bundle',
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
    Benchmark('chrome_java_nosig', {
        'kind': 'incremental_build_and_install',
        'from_string': 'sInstance = instance;',
        'to_string': 'sInstance = instance;String test = "Test";',
        # pylint: disable=line-too-long
        'change_file': 'chrome/android/java/src/org/chromium/chrome/browser/AppHooks.java',
    }),
    Benchmark('chrome_java_sig', {
        'kind': 'incremental_build_and_install',
        'from_string': 'AppHooksImpl sInstance;',
        'to_string': 'AppHooksImpl sInstance;public void NewInterfaceMethod(){}',
        # pylint: disable=line-too-long
        'change_file': 'chrome/android/java/src/org/chromium/chrome/browser/AppHooks.java',
    }),
    Benchmark('chrome_java_res', {
        'kind': 'incremental_build_and_install',
        'from_string': '14181C',
        'to_string': '14181D',
        'change_file': 'chrome/android/java/res/values/colors.xml',
    }),
    Benchmark('module_java_public_sig', {
        'kind': 'incremental_build_and_install',
        'from_string': 'INVALID_WINDOW_INDEX = -1',
        'to_string': 'INVALID_WINDOW_INDEX = -2',
        # pylint: disable=line-too-long
        'change_file': 'chrome/browser/tabmodel/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManager.java',
    }),
    Benchmark('module_java_internal_nosig', {
        'kind': 'incremental_build_and_install',
        'from_string': '"TabModelSelector',
        'to_string': '"DifferentUniqueString',
        # pylint: disable=line-too-long
        'change_file': 'chrome/browser/tabmodel/internal/android/java/src/org/chromium/chrome/browser/tabmodel/TabWindowManagerImpl.java',
    }),
    Benchmark('base_java_nosig', {
        'kind': 'incremental_build_and_install',
        'from_string': '"SysUtil',
        'to_string': '"SysUtil1',
        'change_file': 'base/android/java/src/org/chromium/base/SysUtils.java',
    }),
    Benchmark('base_java_sig', {
        'kind': 'incremental_build_and_install',
        'from_string': 'SysUtils";',
        'to_string': 'SysUtils";public void NewInterfaceMethod(){}',
        'change_file': 'base/android/java/src/org/chromium/base/SysUtils.java',
    }),
    Benchmark('turbine_headers', {
        'kind': 'incremental_build',
        'from_string': '# found in the LICENSE file.',
        'to_string': '#temporary_edit_for_benchmark.py',
        'change_file': 'build/android/gyp/turbine.py',
    }),
    Benchmark('compile_java', {
        'kind': 'incremental_build',
        'from_string': '# found in the LICENSE file.',
        'to_string': '#temporary_edit_for_benchmark.py',
        'change_file': 'build/android/gyp/compile_java.py',
    }),
    Benchmark('write_build_config', {
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


@contextlib.contextmanager
def _server():
    cmd = [_SRC_ROOT / 'build' / 'android' / 'fast_local_dev_server.py']
    # Avoid the build server's output polluting benchmark results, but allow
    # stderr to get through in case the build server fails with an error.
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


def _detect_emulators():
    return [d for d in adb_wrapper.AdbWrapper.Devices() if d.is_emulator]


@contextlib.contextmanager
def _emulator():
    emulators = _detect_emulators()
    assert len(emulators) == 0, 'There must be no existing emulators running.'
    try:
        cmd = [_AVD_SCRIPT, 'start', '-q', '--avd-config', _AVD_CONFIG]
        subprocess.run(cmd, check=True, capture_output=True)
    except subprocess.CalledProcessError:
        print('Unable to start the emulator. Perhaps you need to install it:')
        print(f'{_AVD_SCRIPT} install --avd-config {_AVD_CONFIG}')
        raise
    for sec in range(_SECONDS_TO_WAIT_FOR_EMULATOR_STARTUP):
        emulators = _detect_emulators()
        if len(emulators) >= 1:
            break
        logging.debug(f'Waited {sec}s for emulator to become ready...')
        time.sleep(1)
    else:
        raise Exception('Emulator does not seem to be ready after waiting '
                        f'{_SECONDS_TO_WAIT_FOR_EMULATOR_STARTUP} seconds.')
    assert len(emulators) == 1, 'There are too many emulators running.'
    device = device_utils.DeviceUtils(emulators[0])
    logging.debug(f'Started: {device.serial}.')
    try:
        # Ensure the emulator and its disk are fully set up.
        device.WaitUntilFullyBooted(decrypt=True)
        # TODO(wnwen): Remove once split apks are used instead of side-loading.
        device.adb.Shell('settings put global hidden_api_policy_p_apps 0')
        yield
    finally:
        device.adb.Emu('kill')


def _run_and_time_cmd(cmd: List[str]) -> float:
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


def _run_gn_gen(out_dir: str) -> float:
    return _run_and_time_cmd(['gn', 'gen', '-C', out_dir])


def _run_autoninja(out_dir: str, target: str) -> float:
    return _run_and_time_cmd(['autoninja', '-C', out_dir, target])


def _run_install(out_dir: str, target: str) -> float:
    # Example script path: out/Debug/bin/chrome_public_apk
    script_path = os.path.join(out_dir, 'bin', target)
    # Disable first run to get a more accurate timing of startup.
    return _run_and_time_cmd([
        script_path, 'run', '--args=--disable-fre', '--exit-on-match',
        '^Successfully loaded native library$'
    ])


def _run_and_maybe_install(out_dir: str, target: str,
                           use_emulator: bool) -> float:
    total_time = _run_autoninja(out_dir, target)
    if use_emulator:
        total_time += _run_install(out_dir, target)
    return total_time


def _run_incremental_benchmark(*, out_dir: str, target: str, from_string: str,
                               to_string: str, change_file: str,
                               use_emulator: bool) -> Iterator[float]:
    # This ensures that the only change is the one that this script makes.
    prep_time = _run_and_maybe_install(out_dir, target, use_emulator)
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
        yield _run_and_maybe_install(out_dir, target, use_emulator)
    # Since we are restoring the original file, this is the same incremental
    # change, just reversed, so do a second run to save on prep time. This
    # ensures a minimum of two runs.
    pathlib.Path(change_file_path).touch()
    yield _run_and_maybe_install(out_dir, target, use_emulator)


def _run_benchmark(*, kind: str, use_emulator: bool,
                   **kwargs: Dict) -> Iterator[float]:
    if kind == 'incremental_build':
        assert not use_emulator, f'Install not supported for {kwargs}.'
        return _run_incremental_benchmark(use_emulator=False, **kwargs)
    elif kind == 'incremental_build_and_install':
        return _run_incremental_benchmark(use_emulator=use_emulator, **kwargs)
    else:
        raise NotImplementedError(f'Benchmark type {kind} is not defined.')


def _format_result(time_taken: List[float]) -> str:
    avg_time = sum(time_taken) / len(time_taken)
    list_of_times = ', '.join(f'{t:.1f}s' for t in time_taken)
    result = f'{avg_time:.1f}s'
    if len(time_taken) > 1:
        result += f' avg ({list_of_times})'
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
                   use_emulator: bool) -> Iterator[Tuple[str, List[float]]]:
    out_dir = os.path.relpath(output_directory, _SRC_ROOT)
    args_gn_path = os.path.join(out_dir, 'args.gn')
    emulator_ctx = _emulator() if use_emulator else contextlib.nullcontext()
    server_ctx = _server() if not no_server else contextlib.nullcontext()
    with _backup_file(args_gn_path), emulator_ctx, server_ctx:
        with open(args_gn_path, 'w') as f:
            # Use newlines instead of spaces since autoninja.py uses regex to
            # determine whether use_goma is turned on or off.
            f.write('\n'.join(gn_args))
        yield 'gn gen', [_run_gn_gen(out_dir)]
        for benchmark in _parse_benchmarks(benchmarks):
            logging.info(f'Starting {benchmark.name}...')
            time_taken = []
            for run_num in range(repeat):
                logging.info(f'Run number: {run_num + 1}')
                for elapsed in _run_benchmark(out_dir=out_dir,
                                              target=target,
                                              use_emulator=use_emulator,
                                              **benchmark.info):
                    logging.info(f'Time: {elapsed:.1f}s')
                    time_taken.append(elapsed)
            logging.info(f'Completed {benchmark.name}')
            logging.info('Result: %s', _format_result(time_taken))
            yield benchmark.name, time_taken


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
    parser.add_argument('benchmark',
                        nargs='+',
                        metavar='BENCHMARK',
                        choices=list(_all_benchmark_and_suite_names()),
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
    parser.add_argument(
        '--use-emulator',
        action='store_true',
        help='Use an emulator to include install/launch timing.')
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
    if args.use_emulator:
        devil_chromium.Initialize()
        gn_args += _EMULATOR_GN_ARGS

    target = _TARGETS['bundle' if args.bundle else 'apk']
    results = run_benchmarks(args.benchmark, gn_args, out_dir, target,
                             args.repeat, args.no_server, args.use_emulator)
    server_str = f'{"not " if args.no_server else ""}using build server'
    emulator_str = f'{"" if args.use_emulator else "not "}using emulator'
    print(f'Summary ({server_str}; {emulator_str})')
    print(f'gn args: {" ".join(gn_args)}')
    print(f'target: {target}')
    for name, result in results:
        print(f'{name}: {_format_result(result)}')


if __name__ == '__main__':
    sys.exit(main())

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
import json
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import time
import psutil
import tempfile
from dataclasses import dataclass, field
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

_EXE_EXT = '.exe' if sys.platform == 'win32' else ''
_THIS_DIR = os.path.dirname(__file__)
_ROOT_DIR = f'{_THIS_DIR}/../..'
_UPDATE_PY = f'{_THIS_DIR}/../clang/scripts/update.py'
_LLVM_DIR = f'{_ROOT_DIR}/third_party/llvm-build/Release+Asserts'
_PROFDATA = f'{_LLVM_DIR}/bin/llvm-profdata{_EXE_EXT}'

# Root of the per-package PGO profile directories on an Android device. The
# full path is `{root}/{package}/{_ANDROID_PGO_CACHE_SUFFIX}`. This split is
# needed because crossbench's chromium_pgo probe takes the root and appends the
# package itself, while telemetry's --fetch-data-path-device takes a full path.
_ANDROID_PGO_ROOT = '/data_mirror/data_ce/null/0'
_ANDROID_PGO_CACHE_SUFFIX = 'cache/pgo_profiles'

# Installing a bundle through its wrapper script resolves `java` from PATH,
# see devil's bundletool wrapper in
# third_party/catapult/devil/devil/android/sdk/bundletool.py. Use Chromium's
# hermetic JDK, brought into the isolate by //third_party/jdk:java_data (a
# data_dep of //tools/pgo's script tests).
_JDK_BIN_DIR = f'{_ROOT_DIR}/third_party/jdk/current/bin'

# Telemetry sets these on every benchmark run, on top of the field trial config,
# see GetFromBrowserOptions in //third_party/catapult/telemetry/telemetry/
# internal/backends/chrome/chrome_startup_args.py. Keep them in sync so that
# crossbench profiles the same browser configuration telemetry does.
_TELEMETRY_STARTUP_BROWSER_ARGS = (
    # Stops the hang watcher from generating dumps, see
    # https://crbug.com/425223287.
    '--disable-features=EnableHangWatcher,EnableHangWatcherOnGpuProcess',
)

# Benchmark files to serve from a local HTTP file server, since devices in
# isolated lab environments have no access to the live benchmark URLs. The
# version has to match the one crossbench resolves for the benchmark name, see
# the `aliases()` of the classes in
# third_party/crossbench/crossbench/benchmarks/{speedometer,jetstream}/. Names
# absent from this map keep crossbench's default (live) URL.
_CROSSBENCH_LOCAL_BENCHMARK_DIRS = {
    'speedometer3': 'third_party/speedometer/v3.1',
    'jetstream2': 'third_party/jetstream/v2.2',
}

# This is necessary to get proper logging on bots and locally. If this script is
# run through run_isolated_script_test.py, a root logger would have already been
# set up. Thus for this script's logging to appear (and not disrupt other
# loggers) it needs to use its own logger.
_LOGGER = logging.getLogger(__name__)


def IsStoryFlag(flag: str):
    return flag.startswith('--story') or flag == '--run-abridged-story-set'


@dataclass
class Benchmark:
    '''Describes a benchmark and the set of arguments needed to run it.'''

    name: str
    args: List[str]
    enable_features: List[str] = field(default_factory=list)
    disable_features: List[str] = field(default_factory=list)
    pageset_repeat: int = 1

    def is_crossbench(self) -> bool:
        return self.name.endswith('.crossbench')

    def ReplaceStoryArg(self, story: str):
        copy_args = [a for a in self.args if not IsStoryFlag(a)]
        # Insert story as the second argument to make it easier to understand
        # what the benchmark command is running at a glance.
        copy_args.insert(1, f'--story={story}')
        return Benchmark(
            self.name,
            copy_args,
            self.enable_features.copy(),
            self.disable_features.copy(),
        )

    def ProduceBrowserArgs(
        self, extra_disabled_features: Optional[List[str]] = None
    ):
        '''Returns the browser flags this benchmark needs.'''
        if extra_disabled_features is None:
            extra_disabled_features = []

        all_disabled_features = self.disable_features + extra_disabled_features

        intersect_features = set(self.enable_features).intersection(
            all_disabled_features
        )
        if intersect_features:
            raise RuntimeError(
                f'Features {intersect_features} were both enabled and disabled.'
            )

        browser_args = []
        if self.enable_features:
            browser_args.append(
                '--enable-features=' + ','.join(self.enable_features)
            )
        if all_disabled_features:
            browser_args.append(
                '--disable-features=' + ','.join(all_disabled_features)
            )
        return browser_args

    def ProduceBenchmarkArgs(
        self,
        extra_disabled_features: Optional[List[str]] = None,
        browser_args: Optional[List[str]] = None,
    ):
        if any(a.startswith('--extra-browser-args') for a in self.args):
            raise RuntimeError(
                '--extra-browser-args was added to benchmark args.'
            )

        if browser_args is None:
            browser_args = self.ProduceBrowserArgs(extra_disabled_features)

        final_args = self.args.copy()
        if browser_args:
            # No quotes around the space separated arguments is needed.
            final_args.append(f'--extra-browser-args={" ".join(browser_args)}')

        return final_args


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
    # Not an argument: the package of `android_browser`, resolved in
    # parse_args() and only set when `android_browser` is set.
    # TODO(crbug.com/479547498): Plumb this separately instead, or create a new
    # object that is not OptionsNamespace.
    android_package: Optional[str]
    skip_profdata: bool
    run_public_benchmarks_only: bool
    temporal_trace_length: Optional[int]
    separate_renderer_pgo: bool
    repeats: int
    verbose: int
    quiet: int
    # The following are bot-specific args.
    isolated_script_test_output: Optional[str]
    isolated_script_test_perf_output: Optional[str]
    android_hostname: str


def parse_args():
    parser = argparse.ArgumentParser(
        epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    # ▼▼▼▼▼ Please update OptionsNamespace when adding or modifying args. ▼▼▼▼▼
    parser.add_argument(
        '-C', '--builddir', help='Path to build directory.', required=True
    )
    parser.add_argument(
        '--outputdir', help='Path to store final outputs, default is builddir.'
    )
    parser.add_argument(
        '--profiledir',
        help='Path to store temporary profiles, default is builddir/profile.',
    )
    parser.add_argument(
        '--keep-temps',
        action='store_true',
        default=False,
        help='Whether to keep temp files',
    )
    parser.add_argument(
        '--android-browser',
        help='The type of android browser to test, e.g. '
        'android-trichrome-chrome-google-bundle.',
    )
    parser.add_argument(
        '--android-device-path',
        help='The device path to pull profiles from. By '
        'default this is /data_mirror/data_ce/null/0/<package>'
        '/cache/pgo_profiles/ but you can override it for your '
        'device if needed. Use "auto" for dynamic detection.',
    )
    parser.add_argument(
        '--skip-profdata',
        action='store_true',
        default=False,
        help='Only run benchmarks and skip merging profile '
        'data. Used for sample-based profiling for Propeller '
        'and BOLT',
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        default=False,
        help='Skip running the benchmarks.',
    )
    parser.add_argument(
        '--run-public-benchmarks-only',
        action='store_true',
        default=False,
        help='Only run benchmarks that do not require any special access. See '
        'https://www.chromium.org/developers/telemetry/upload_to_cloud_storage/#request-access-for-google-partners '
        'for more information.',
    )
    # TODO(crbug.com/479547498): Remove this option and run
    # jetstream3.crossbench by default after we finish testing.
    parser.add_argument(
        '--run-jetstream3',
        '--run-js3',
        action='store_true',
        default=False,
        help='Include JetStream 3 benchmark (using crossbench)',
    )
    parser.add_argument(
        '--temporal-trace-length',
        type=int,
        help='Add flags necessary for temporal PGO (experimental).',
    )
    parser.add_argument(
        '--separate-renderer-pgo',
        action='store_true',
        default=False,
        help='Generate a separate PGO profile for the renderer binary.',
    )
    parser.add_argument(
        '-r',
        '--repeats',
        type=int,
        default=5,
        help='Number of times to attempt each benchmark if it fails, default 5',
    )
    parser.add_argument(
        '-v',
        '--verbose',
        action='count',
        default=0,
        help='Increase verbosity level (repeat as needed)',
    )
    parser.add_argument(
        '-q',
        '--quiet',
        action='count',
        default=0,
        help='Decrease verbosity level (passed through to run_benchmark.)',
    )
    parser.add_argument(
        '--isolated-script-test-output',
        help='Output.json file that the script can write to.',
    )
    parser.add_argument(
        '--isolated-script-test-perf-output',
        help='Deprecated and ignored, but bots pass it.',
    )
    parser.add_argument(
        "--android-hostname", help="Run benchmarks with adb hostname."
    )
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

    args.android_package = None
    if args.android_browser:
        _LOGGER.info(f"Android browser: {args.android_browser}")
        for settings in _ANDROID_SETTINGS:
            if settings.browser_type == args.android_browser:
                args.android_package = settings.package
                break
        else:
            raise ValueError(f'Unable to find {args.android_browser} settings.')

        if not args.android_device_path:
            args.android_device_path = (
                f'{_ANDROID_PGO_ROOT}/'
                f'{args.android_package}/'
                f'{_ANDROID_PGO_CACHE_SUFFIX}'
            )
            _LOGGER.info(
                f"Using default Android device path: {args.android_device_path}"
            )
        else:
            _LOGGER.info(
                "Using provided Android device path: "
                f"{args.android_device_path}"
            )

    if not args.profiledir:
        args.profiledir = f'{args.builddir}/profile'

    if not args.outputdir:
        args.outputdir = args.builddir

    if args.isolated_script_test_output:
        args.outputdir = os.path.dirname(args.isolated_script_test_output)

    _LOGGER.info(f"Output directory: {args.outputdir}")
    _LOGGER.info(f"Profile directory: {args.profiledir}")

    return args


def run_profdata_merge(output_path, input_files, args: OptionsNamespace):
    _LOGGER.info(f"Merging {len(input_files)} profile files into {output_path}")
    if args.temporal_trace_length:
        extra_args = [
            '--temporal-profile-max-trace-length',
            str(args.temporal_trace_length),
        ]
        _LOGGER.debug(
            f"Using temporal trace length: {args.temporal_trace_length}"
        )

    else:
        extra_args = []

    cmd = [_PROFDATA, 'merge', '-o', output_path] + extra_args + input_files
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


def get_crossbench_local_file_server_args(benchmark_name: str):
    '''Returns the --local-file-server args for a crossbench benchmark.'''
    local_dir = _CROSSBENCH_LOCAL_BENCHMARK_DIRS.get(benchmark_name)
    if not local_dir:
        raise FileNotFoundError(f"Local files for {benchmark_name} are missing")

    local_path = os.path.abspath(f'{_ROOT_DIR}/{local_dir}')
    if not os.path.isdir(local_path):
        raise FileNotFoundError(
            f'{local_path} is missing, cannot serve {benchmark_name} locally. '
            'It should be brought in by //tools/perf:perf, see '
            '//tools/perf/BUILD.gn.'
        )
    return [f'--local-file-server={local_path}']


def get_crossbench_browser_path(args: OptionsNamespace):
    '''Returns the browser wrapper script to pass to crossbench.

    Crossbench only accepts wrapper names it knows, see
    CHROME_APK_HELPER_NAMES in
    third_party/crossbench/crossbench/cli/config/apk_helper.py. All browsers
    used for PGO generation are in that list.
    '''
    assert args.android_browser
    name = args.android_browser.removeprefix('android-').replace('-', '_')
    path = os.path.join(args.builddir, 'bin', name)
    if not os.path.exists(path):
        raise FileNotFoundError(
            f'{path} does not exist, is {args.android_browser} built?'
        )
    return path


def get_crossbench_driver_path(args: OptionsNamespace):
    '''Returns the chromedriver crossbench should use, if it was built.'''
    # //tools/perf:perf depends on chromedriver for the host toolchain, which
    # for Android builds ends up in the host toolchain subdirectory.
    for path in (
        f'{args.builddir}/clang_x64/chromedriver{_EXE_EXT}',
        f'{args.builddir}/chromedriver{_EXE_EXT}',
    ):
        if os.path.exists(path):
            return path

    raise FileNotFoundError(f"chromedriver not found for {args.builddir}")


def build_crossbench_flags_dict(browser_args: List[str]):
    '''Turns `--flag` / `--flag=value` strings into crossbench's dict form.

    Valueless flags map to ''. Rejects anything else (e.g. a space-separated
    `--flag value` pair, which would arrive here as two arguments) instead of
    letting it silently turn into a broken flag, and rejects conflicting
    duplicates instead of silently keeping the last value.
    '''
    flags = {}
    for flag in browser_args:
        name, _, value = flag.partition('=')
        if not name.startswith('-') or ' ' in name:
            raise ValueError(
                f'Browser flag {flag!r} is not in --flag[=value] '
                'form, cannot pass it to crossbench.'
            )
        if name in flags and flags[name] != value:
            raise ValueError(
                f'Browser flag {name} set twice, with values '
                f'{flags[name]!r} and {value!r}.'
            )
        flags[name] = value
    return flags


def write_crossbench_browser_config(
    args: OptionsNamespace, browser_args: List[str], config_path: str
):
    '''Writes a crossbench browser config and returns the args selecting it.'''
    # TODO(crbug.com/479547498): Support passing flags directly as a list in
    # crossbench's browser config. Currently, inline strings starting with "--"
    # are parsed with a regex that fails on complex flags like
    # --force-fieldtrial-params, whereas a top-level {name: value} flag group
    # skips regex parsing.
    # Note: crossbench's ChromeFeatures drops the `<Study` association from
    # --disable-features entries (it keeps it for --enable-features). The
    # features stay disabled and the studies are still forced via
    # --force-fieldtrials, only the trial-activation attribution of the
    # disabled features is lost, which does not affect the profiled code.
    driver = {
        'type': 'adb',
        'path': get_crossbench_driver_path(args),
    }
    if args.android_hostname:
        driver['device_id'] = args.android_hostname

    config = {
        'flags': {
            'pgo': build_crossbench_flags_dict(browser_args),
        },
        'browsers': {
            'pgo': {
                'path': get_crossbench_browser_path(args),
                'driver': driver,
                'flags': ['pgo'],
            },
        },
    }
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=2)
    _LOGGER.debug(f'Wrote crossbench browser config to {config_path}')
    return [f'--browser-config={config_path}']


def get_crossbench_variations_browser_args(browser_args: List[str]):
    '''Returns `browser_args` plus the field trials telemetry applies.'''
    # Telemetry enables the field trial testing config for every perf benchmark.
    # On Android it cannot use --enable-field-trial-config, because the config's
    # compiled out of Chrome-branded Android builds (FIELDTRIAL_TESTING_ENABLED,
    # see //components/variations/service/BUILD.gn), so it expands the config
    # into explicit flags on the host instead. Do exactly the same here,
    # otherwise crossbench would profile a different set of features than
    # telemetry did. See PerfBenchmark._GetVariationsBrowserArgs in
    # //tools/perf/core/perf_benchmark.py.
    variations_dir = os.path.abspath(f'{_ROOT_DIR}/tools/variations')
    if variations_dir not in sys.path:
        sys.path.append(variations_dir)
    # pylint: disable=import-outside-toplevel,import-error
    import fieldtrial_util

    config_path = os.path.abspath(
        f'{_ROOT_DIR}/testing/variations/fieldtrial_testing_config.json'
    )
    # Passing browser_args makes GenerateArgs skip the experiments that conflict
    # with the features this script sets.
    generated_args = fieldtrial_util.GenerateArgs(
        config_path, 'android', browser_args
    )
    if not generated_args:
        raise RuntimeError(f'No field trial args generated from {config_path}.')

    # Merge, so that the duplicate --enable-features/--disable-features coming
    # from the three sources become a single flag each. Chrome keeps only the
    # last value of a repeated switch, so leaving them separate would silently
    # drop features. Telemetry adds _TELEMETRY_STARTUP_BROWSER_ARGS after
    # generating the field trial args, so they are deliberately not part of the
    # override set above.
    return fieldtrial_util.MergeFeaturesAndFieldTrialsArgs(
        browser_args + generated_args + list(_TELEMETRY_STARTUP_BROWSER_ARGS)
    )


def add_java_to_path(env: dict):
    '''Puts Chromium's hermetic JDK on PATH for the browser wrapper script.

    The browser wrapper script calls devil's bundletool wrapper to install
    bundles, which resolves `java` from PATH. Crossbench itself resolves `adb`
    from Chromium's checkout without needing it on PATH.
    '''
    tool_dir = os.path.abspath(_JDK_BIN_DIR)
    if not os.path.exists(os.path.join(tool_dir, f'java{_EXE_EXT}')):
        # Not fatal: a tool already on PATH still works.
        _LOGGER.warning(
            'No java in %s, relying on PATH already having one.',
            tool_dir,
        )
        return
    env['PATH'] = tool_dir + os.pathsep + env.get('PATH', '')
    _LOGGER.debug(f'Prepended {tool_dir} to PATH for crossbench')


def get_crossbench_pgo_probe_args(args: OptionsNamespace):
    '''Returns the --probe args that make crossbench collect PGO profiles.'''
    # Without this probe nothing dumps the profiles: crossbench tears the
    # browser down with `am force-stop`, which kills the renderers before they
    # can write their .profraw files. The probe triggers
    # NativeProfiling.dumpProfilingDataOfAllProcesses over DevTools and pulls
    # the result, all of it before the browser's data directory goes away (the
    # apk is uninstalled at the end of a run). This is the crossbench
    # equivalent of telemetry's CHROME_PGO_PROFILING + --fetch-device-data.
    assert args.android_device_path and args.android_package
    suffix = f'/{args.android_package}/{_ANDROID_PGO_CACHE_SUFFIX}'
    if not args.android_device_path.endswith(suffix):
        raise ValueError(
            f'--android-device-path={args.android_device_path} is not '
            f'supported with crossbench benchmarks: it has to end with '
            f'{suffix}, since crossbench\'s chromium_pgo probe appends that '
            'to the root path itself.'
        )
    root = args.android_device_path[: -len(suffix)]
    return [f'--probe=chromium_pgo:{{remote_pgo_root_path: "{root}"}}']


def run_benchmark(benchmark: Benchmark, args: OptionsNamespace):
    '''Puts profdata in {profiledir}/{args[0]}.profdata'''
    global _android_browser_installed

    is_crossbench = benchmark.is_crossbench()

    disabled_features = [
        # Disabling spare renderer features when profiling prevent dumping
        # profile data too early during benchmarks which would result in
        # incomplete profraw files. See https://crbug.com/366235732.
        'SpareRendererForSitePerProcess',
        'AndroidWarmUpSpareRendererWithTimeout',
    ]

    # With crossbench, Android browser flags go into a crossbench browser
    # config instead, see write_crossbench_browser_config().
    uses_browser_config = is_crossbench and args.android_browser
    browser_args = benchmark.ProduceBrowserArgs(disabled_features)
    if uses_browser_config:
        browser_args = get_crossbench_variations_browser_args(browser_args)
    benchmark_args = benchmark.ProduceBenchmarkArgs(
        browser_args=[] if uses_browser_config else browser_args
    )

    pageset_repeat_str = (
        f' with pageset_repeat={benchmark.pageset_repeat}'
        if benchmark.pageset_repeat != 1
        else ''
    )
    _LOGGER.info(
        f"Running benchmark: {' '.join(benchmark_args)}{pageset_repeat_str}"
    )

    # Include the story since per-story benchmarks use [name, --story=s]. The
    # remaining args are browser flags, which are far too long to name a
    # directory after now that they carry the field trial config.
    name = benchmark_args[0]
    if len(benchmark_args) > 1 and IsStoryFlag(benchmark_args[1]):
        name += f'_{benchmark_args[1]}'

    # Clean up intermediate files from previous runs.
    profraw_path = f'{args.profiledir}/{name}/raw'
    _LOGGER.debug(f"Raw profile path: {profraw_path}")

    if os.path.exists(profraw_path):
        _LOGGER.debug(
            f"Removing existing raw profile directory: {profraw_path}"
        )
        shutil.rmtree(profraw_path)
    os.makedirs(profraw_path, exist_ok=True)

    profdata_path = f'{args.profiledir}/{name}.profdata'
    _LOGGER.debug(f"profdata path: {profdata_path}")
    if os.path.exists(profdata_path):
        _LOGGER.debug(f"Removing existing profdata file: {profdata_path}")
        os.remove(profdata_path)

    env = os.environ.copy()
    if args.android_browser:
        if is_crossbench:
            add_java_to_path(env)
        else:
            # Read by telemetry's android_browser_backend to dump the profiles
            # before closing the browser. Crossbench uses the chromium_pgo
            # probe instead, see get_crossbench_pgo_probe_args().
            env['CHROME_PGO_PROFILING'] = '1'
            _LOGGER.debug("Set environment variable CHROME_PGO_PROFILING=1")
    else:
        env['LLVM_PROFILE_FILE'] = f'{profraw_path}/default-%2m.profraw'
        _LOGGER.debug(
            "Set environment variable "
            f"LLVM_PROFILE_FILE={env['LLVM_PROFILE_FILE']}"
        )

    if is_crossbench:
        cmd = (
            ['vpython3', 'third_party/crossbench/cb.py']
            + benchmark_args
            + [
                '-r',
                str(benchmark.pageset_repeat),
                '--no-splash',
                # Crossbench otherwise fills its output directory with symlinked
                # views of the same results (per run/session/story). Those make
                # the profraw glob below return each file hundreds of times
                # over, and merging a profile with itself multiplies its
                # counters by a factor that depends on the benchmark's story
                # count, which skews how the benchmarks are weighted against
                # each other in the final profile.
                '--no-symlinks',
                # Crossbench asserts that its output directory does not exist
                # yet, so point it inside (not at) the already created
                # profraw_path.
                '--out-dir',
                f'{profraw_path}/crossbench',
            ]
        )
        # In isolated lab environments (e.g. Android trybots), devices do not
        # have internet access to the live benchmark URLs, so serve the
        # benchmark files from a local HTTP file server instead.
        cmd += get_crossbench_local_file_server_args(benchmark_args[0])
    else:
        cmd = (
            ['vpython3', 'tools/perf/run_benchmark']
            + benchmark_args
            + [
                f'--chromium-output-directory={args.builddir}',
                '--assert-gpu-compositing',
                f'--pageset-repeat={benchmark.pageset_repeat}',
                # Abort immediately when any story fails, since a failed story
                # fails to produce valid profdata. Fail fast and rely on repeats
                # to get a valid profdata.
                '--max-failures=0',
            ]
        )

    # Add N copies of verbose/quiet flag
    cmd += ['-v'] * args.verbose + ['-q'] * args.quiet

    if args.android_browser:
        if is_crossbench:
            cmd += write_crossbench_browser_config(
                args, browser_args, f'{args.profiledir}/browser_config.json'
            )
            cmd += get_crossbench_pgo_probe_args(args)
        else:
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

            if args.android_hostname:
                cmd += [
                    "--connect-to-device-over-network",
                    f"--device={args.android_hostname}",
                ]

        _LOGGER.debug(
            f"Running benchmark on Android with command: {' '.join(cmd)}"
        )
    else:
        if sys.platform == 'darwin':
            exe_path = f'{args.builddir}/Chromium.app/Contents/MacOS/Chromium'
        else:
            exe_path = f'{args.builddir}/chrome' + _EXE_EXT
        if is_crossbench:
            driver_path = f'{args.builddir}/chromedriver' + _EXE_EXT
            cmd += ['-b', exe_path, '--driver-path', driver_path]
        else:
            cmd += [
                '--browser=exact',
                f'--browser-executable={exe_path}',
            ]

        _LOGGER.debug(
            f"Running benchmark locally with command: {' '.join(cmd)}"
        )

    if not args.dry_run:
        subprocess.run(
            cmd,
            check=True,
            shell=sys.platform == 'win32',
            env=env,
            cwd=_ROOT_DIR,
        )

    # When using separate renderer binaries, child processes (e.g., renderers
    # or helpers) need time to exit cleanly so that LLVM profile handlers
    # write out all .profraw files before classification and merging begin.
    if not args.dry_run:
        builddir_abs = os.path.abspath(args.builddir).lower()
        _LOGGER.info(
            f"Waiting for all child processes in {args.builddir} to exit..."
        )
        max_wait_seconds = 30
        start_time = time.time()
        while time.time() - start_time < max_wait_seconds:
            running_processes = []
            for proc in psutil.process_iter(['name', 'exe', 'pid']):
                try:
                    exe_path = proc.info['exe']
                    if exe_path and os.path.abspath(
                        exe_path
                    ).lower().startswith(builddir_abs):
                        running_processes.append(proc.info['pid'])
                except (
                    psutil.NoSuchProcess,
                    psutil.AccessDenied,
                    psutil.ZombieProcess,
                ):
                    pass
                except Exception:
                    pass
            if not running_processes:
                break
            _LOGGER.info(
                f"Still waiting for child PIDs to exit: {running_processes}..."
            )
            time.sleep(1)
        else:
            _LOGGER.warning(
                f"Timed out waiting for child processes to exit after "
                f"{max_wait_seconds}s: {running_processes}"
            )

    if args.skip_profdata:
        _LOGGER.info("Skipping profdata merging")

        return

    # Android's `adb pull` does not allow * globbing (i.e. pulling
    # pgo_profiles/*). Since the naming of profraw files can vary, pull the
    # directory and check subdirectories recursively for .profraw files.
    profraw_files = glob.glob(f'{profraw_path}/**/*.profraw', recursive=True)
    if is_crossbench:
        # Deduplicate by real path: crossbench's output directory may contain
        # symlinked views of the same results, and passing the same profile to
        # llvm-profdata more than once silently multiplies its counters
        # instead of failing.
        profraw_files = sorted({os.path.realpath(p) for p in profraw_files})
    _LOGGER.debug(f"Found {len(profraw_files)} profraw files")
    if not profraw_files:
        raise RuntimeError(f'No profraw files found in {profraw_path}')

    if not args.separate_renderer_pgo:
        run_profdata_merge(profdata_path, profraw_files, args)
    else:
        # Group profraw files by the prefix before the first '-' in their
        # basename.
        # e.g., 'default-12345.profraw' -> prefix 'default'
        # e.g., 'renderer-12345.profraw' -> prefix 'renderer'
        files_by_prefix = {}
        for f in profraw_files:
            filename = os.path.basename(f)
            prefix = (
                filename.split('-', 1)[0] if '-' in filename else filename[:-8]
            )
            files_by_prefix.setdefault(prefix, []).append(f)

        # Process the 'default' (main browser process) files first
        default_files = files_by_prefix.pop('default', [])
        if default_files:
            run_profdata_merge(profdata_path, default_files, args)
        else:
            raise RuntimeError(f'No browser profile files found for {name}')

        # Perform one merge per remaining prefix
        if files_by_prefix:
            for prefix, prefix_files in files_by_prefix.items():
                target_profdata_path = profdata_path.replace(
                    '.profdata', f'_{prefix}.profdata'
                )
                run_profdata_merge(target_profdata_path, prefix_files, args)
        else:
            _LOGGER.warning(f'No non-default profile files found for {name}')

    # Test merge to prevent issues like: https://crbug.com/353702041
    with tempfile.NamedTemporaryFile() as f:
        _LOGGER.debug("Testing profdata merge")

        run_profdata_merge(f.name, [profdata_path], args)


def run_benchmark_with_repeats(benchmark: Benchmark, args: OptionsNamespace):
    '''Runs the benchmark with provided args, return # of times it failed.'''
    assert args.repeats > 0, 'repeats must be at least 1'
    for idx in range(args.repeats):
        try:
            _LOGGER.info(f"Running benchmark attempt {idx + 1}/{args.repeats}")

            run_benchmark(benchmark, args)
            _LOGGER.info(f"Benchmark succeeded on attempt {idx + 1}")

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
                    f'Retry attempt {idx + 1} for '
                    f'{benchmark.ProduceBenchmarkArgs()}'
                )
            else:
                _LOGGER.error(f'Failed {args.repeats} times')
                raise e
    # This statement can never be reached due to the above `raise e` but is here
    # to appease the typechecker.
    return args.repeats


def get_stories(benchmark: Benchmark, args: OptionsNamespace):
    _LOGGER.info(f"Getting stories for benchmark: {' '.join(benchmark.args)}")
    print_stories_cmd = (
        [
            'vpython3',
            'tools/perf/run_benchmark',
        ]
        + benchmark.args
        + [
            '--print-only=stories',
            # This is essential to skip filtered stories.
            '--print-only-runnable',
            f'--browser={args.android_browser}',
            '-vv',
        ]
    )
    if args.android_hostname:
        print_stories_cmd += [
            "--connect-to-device-over-network",
            f"--device={args.android_hostname}",
        ]
    _LOGGER.debug(f"Running command: {' '.join(print_stories_cmd)}")

    # Avoid setting check=True here since the return code is 111 for success.
    proc = subprocess.run(
        print_stories_cmd, text=True, capture_output=True, cwd=_ROOT_DIR
    )

    stories = []
    for line in proc.stdout.splitlines():
        if line and not line.startswith(('[', 'View results at')):
            stories.append(line)
    _LOGGER.debug(f"Found {len(stories)} stories")
    return stories


def run_benchmarks(benchmarks: List[Benchmark], args: OptionsNamespace):
    fail_count = 0
    for benchmark in benchmarks:
        _LOGGER.info(f"Starting benchmark: {benchmark.name}")
        # Telemetry wipes /data/data/<package>/* when it sets up the browser for
        # a story, which drops the profiles collected by the preceding story. On
        # Android its benchmarks are therefore split into one run per story, so
        # that every story's profile is kept (see https://crrev.com/c/5718532).
        # Crossbench benchmarks do not need this: speedometer3 and jetstream2
        # run all their substories within a single page load, and the
        # chromium_pgo probe collects the profiles per run, before the browser
        # is torn down.
        if not args.android_browser or benchmark.is_crossbench():
            fail_count += run_benchmark_with_repeats(benchmark, args)
        else:
            stories = get_stories(benchmark, args)
            for story in stories:
                _LOGGER.info(f"Running story: {story}")
                story_benchmark = benchmark.ReplaceStoryArg(story)
                fail_count += run_benchmark_with_repeats(story_benchmark, args)
    return fail_count


def merge_profdata(
    profile_output_path: str,
    args: OptionsNamespace,
    benchmarks: Optional[List[Benchmark]] = None,
):
    _LOGGER.info(f"Merging all profdata files into: {profile_output_path}")
    all_profdata = glob.glob(f'{args.profiledir}/*.profdata')

    if not getattr(args, 'separate_renderer_pgo', False):
        _LOGGER.debug(f"Found {len(all_profdata)} profdata files")
        if not all_profdata:
            raise RuntimeError(f'No profdata files found in {args.profiledir}')
        run_profdata_merge(profile_output_path, all_profdata, args)
    else:
        benchmark_names = {b.name for b in benchmarks} if benchmarks else set()
        default_profdata = []
        files_by_prefix = {}
        for f in all_profdata:
            basename = os.path.basename(f)
            stem = (
                basename[: -len('.profdata')]
                if basename.endswith('.profdata')
                else basename
            )
            if stem in benchmark_names:
                default_profdata.append(f)
                continue
            matched = False
            for b_name in sorted(benchmark_names, key=len, reverse=True):
                if stem.startswith(b_name + '_'):
                    prefix = stem[len(b_name) + 1 :]
                    files_by_prefix.setdefault(prefix, []).append(f)
                    matched = True
                    break
            if not matched:
                if '_' in stem:
                    prefix = stem.split('_')[-1]
                    files_by_prefix.setdefault(prefix, []).append(f)
                else:
                    default_profdata.append(f)

        _LOGGER.debug(f"Found {len(default_profdata)} browser profdata files")
        if not default_profdata:
            raise RuntimeError(
                f'No browser profdata files found in {args.profiledir}'
            )
        run_profdata_merge(profile_output_path, default_profdata, args)

        for prefix, files in files_by_prefix.items():
            _LOGGER.debug(f"Found {len(files)} {prefix} profdata files")
            target_output_path = profile_output_path.replace(
                '.profdata', f'_{prefix}.profdata'
            )
            run_profdata_merge(target_output_path, files, args)

    if args.temporal_trace_length:
        _LOGGER.info("Generating orderfile for temporal PGO")
        orderfile_cmd = [
            _PROFDATA,
            'order',
            profile_output_path,
            '-o',
            f'{args.outputdir}/orderfile.txt',
        ]
        _LOGGER.debug(f"Running command: {' '.join(orderfile_cmd)}")

        subprocess.run(orderfile_cmd, check=True)


def main():
    args = parse_args()

    handler = logging.StreamHandler()
    formatter = logging.Formatter(
        '%(levelname).1s %(relativeCreated)6d %(message)s'
    )
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
                check=True,
            )
    assert os.path.exists(_PROFDATA), f'{_PROFDATA} does not exist'

    if os.path.exists(args.profiledir):
        _LOGGER.warning('Removing %s', args.profiledir)
        shutil.rmtree(args.profiledir)

    # Run the shortest benchmarks first to fail early if anything is wrong.
    benchmarks: list[Benchmark] = [
        Benchmark('speedometer3.crossbench', ['speedometer3']),
        Benchmark('jetstream2.crossbench', ['jetstream2']),
    ]

    if args.run_jetstream3:
        benchmarks.append(Benchmark('jetstream3.crossbench', ['jetstream3']))

    # These benchmarks require special access permissions:
    # https://www.chromium.org/developers/telemetry/upload_to_cloud_storage/#request-access-for-google-partners
    if not args.run_public_benchmarks_only:
        platform = 'mobile' if args.android_browser else 'desktop'
        benchmarks.append(
            Benchmark(
                'system_health',
                [
                    f'system_health.common_{platform}',
                    '--run-abridged-story-set',
                ],
            )
        )

        motionmark_benchmark_args = [
            f'rendering.{platform}',
            '--also-run-disabled-tests',
            '--story-tag-filter=motionmark_fixed_2_seconds',
        ]

        # Android arm32 runs on older phones so these benchmarks should only run
        # for arm64.
        if platform == 'mobile' and '64' in args.android_browser:
            # Exercise the Skia Graphite/Dawn/Vulkan path.
            benchmarks.append(
                Benchmark(
                    'motionmark_graphite_dawn_vk',
                    motionmark_benchmark_args,
                    enable_features=['SkiaGraphite'],
                )
            )

            # Exercise the Skia Ganesh/Vulkan path.
            benchmarks.append(
                Benchmark(
                    'motionmark_ganesh_vk',
                    motionmark_benchmark_args,
                    disable_features=['SkiaGraphite'],
                )
            )

            # Exercise the Skia Ganesh/GL on top of ANGLE/GLES path. This is the
            # common path used on most phones without Vulkan support.
            benchmarks.append(
                Benchmark(
                    'motionmark_ganesh_gl',
                    args=motionmark_benchmark_args,
                    enable_features=['DefaultPassthroughCommandDecoder'],
                    disable_features=[
                        'Vulkan',
                        'SkiaGraphite',
                        'DefaultANGLEVulkan',
                    ],
                )
            )
        else:
            benchmarks.append(
                Benchmark('motionmark', motionmark_benchmark_args)
            )

    fail_count = run_benchmarks(benchmarks, args)
    if fail_count:
        _LOGGER.warning(
            f'Of the {len(benchmarks)} benchmarks, there were '
            f'{fail_count} failures that were resolved by repeat '
            'runs.'
        )

    if not args.skip_profdata:
        # Bots run a separate merge step (merge_results.py) that expects profraw
        # files instead of profdata files.
        suffix = ".profraw" if args.isolated_script_test_output else ".profdata"
        profile_output_path = f'{args.outputdir}/profile{suffix}'
        merge_profdata(profile_output_path, args, benchmarks)

    if not args.keep_temps:
        _LOGGER.info(
            'Cleaning up %s, use --keep-temps to keep it.', args.profiledir
        )
        shutil.rmtree(args.profiledir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Script to generate PGO profiles for WebView
'''

import argparse
import glob
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Optional
import pathlib

import websocket

_THIS_DIR = os.path.dirname(__file__)
_SRC_PATH = pathlib.Path(__file__).resolve().parents[2]

# This is necessary to get proper logging on bots and locally. If this script is
# run through run_isolated_script_test.py, a root logger would have already been
# set up. Thus for this script's logging to appear (and not disrupt other
# loggers) it needs to use its own logger.
_LOGGER = logging.getLogger(__name__)

sys.path.append(str(_SRC_PATH / 'third_party/catapult/devil'))
from devil.android import device_utils

sys.path.append(_THIS_DIR)
from generate_profile import (MergeError, run_profdata_merge, merge_profdata,
                              _PROFDATA, _UPDATE_PY, _LLVM_DIR)


# Use this custom Namespace to provide type checking and type hinting.
class OptionsNamespace(argparse.Namespace):
    builddir: str
    # Technically profiledir and outputdir default to `None`, but they are
    # always set before parse_args returns, so leave it as `str` to avoid type
    # errors for methods that take an OptionsNamespace instance.
    outputdir: str
    profiledir: str
    profile_target: str
    webview_build_target: str
    keep_temps: bool
    skip_profdata: bool
    temporal_trace_length: Optional[int]
    verbose: int
    # The following are bot-specific args.
    isolated_script_test_output: Optional[str]
    isolated_script_test_perf_output: Optional[str]


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
    parser.add_argument(
        '--profile-target',
        choices=['agsa', 'gma', 'combined'],
        default='agsa',
        help='The target app to profile (AGSA, GMA, or both Combined).')
    parser.add_argument(
        '--webview-build-target',
        default='system_webview_google_64_32_bundle',
        help=
        'The WebView installer/bundle target to use. (default: %(default)s)')
    parser.add_argument('--keep-temps',
                        action='store_true',
                        default=False,
                        help='Whether to keep temp files')
    parser.add_argument('--skip-profdata',
                        action='store_true',
                        default=False,
                        help='Only run benchmarks and skip merging profile '
                        'data. Used for sample-based profiling for Propeller '
                        'and BOLT')
    parser.add_argument(
        '--temporal-trace-length',
        type=int,
        help='Add flags necessary for temporal PGO (experimental).')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='Increase verbosity level (repeat as needed)')
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

    if not args.profiledir:
        args.profiledir = f'{args.builddir}/profile'

    if not args.outputdir:
        args.outputdir = args.builddir

    if args.isolated_script_test_output:
        args.outputdir = os.path.dirname(args.isolated_script_test_output)

    _LOGGER.info(f"Output directory: {args.outputdir}")
    _LOGGER.info(f"Profile directory: {args.profiledir}")

    return args


def install_and_set_provider(args: OptionsNamespace):
    installer_path = f'{args.builddir}/bin/{args.webview_build_target}'
    _LOGGER.info(f"Installing WebView from {installer_path}")
    subprocess.run([installer_path, 'install'], check=True)
    _LOGGER.info("Setting WebView provider")
    subprocess.run([installer_path, 'set-webview-provider'], check=True)


def trigger_dump(port=9222):
    ws_url = f'ws://localhost:{port}/devtools/browser'
    _LOGGER.info(f"Connecting to {ws_url}...")
    try:
        # Suppress Origin header to bypass 403 Forbidden
        ws = websocket.create_connection(ws_url,
                                         suppress_origin=True,
                                         timeout=5)
        _LOGGER.info(
            "Connected. Sending NativeProfiling.dumpProfilingDataOfAllProcesses"
        )
        request = {
            "id": 1,
            "method": "NativeProfiling.dumpProfilingDataOfAllProcesses"
        }
        ws.send(json.dumps(request))
        response = ws.recv()
        _LOGGER.info(f"Response: {response}")
        ws.close()
        return True
    except Exception as e:
        _LOGGER.error(f"Failed to connect or send command: {e}")
        _LOGGER.error("Ensure the app is debuggable "
                      "(setWebContentsDebuggingEnabled(true)).")
        return False


def get_pid(device, package_name):
    pids = device.GetApplicationPids(package_name)
    return str(pids[0]) if pids else None


def forward_socket(device, pid, local_port=9222):
    socket_name = f"webview_devtools_remote_{pid}"
    _LOGGER.info(
        f"Forwarding local port {local_port} to remote abstract socket "
        f"{socket_name}")
    device.adb.Forward(f'tcp:{local_port}',
                       f'localabstract:{socket_name}',
                       allow_rebind=True)


def clear_remote_profiles(device, device_profiles_dir):
    _LOGGER.info(f"Clearing existing profiles in {device_profiles_dir}...")
    try:
        device.RunShellCommand(f'rm -rf {device_profiles_dir}*',
                               shell=True,
                               check_return=True,
                               as_root=True)
    except Exception:
        _LOGGER.warning(
            "Failed to clear remote profiles. Directory might be empty or "
            "missing.")


def launch_agsa(args: OptionsNamespace):
    _LOGGER.info("Launching AGSA search results page...")
    arch = 'arm64'
    driver_path_arg = [
        f'--driver-path={os.path.join(args.builddir, "clang_x64", "chromedriver")}'
    ]
    wpr_bin_path = os.path.join(_SRC_PATH, 'third_party', 'webpagereplay',
                                'cipd', 'bin', 'linux', 'x86_64', 'wpr')
    wpr_bin_path_arg_fragment = f',"wpr_go_bin":"{wpr_bin_path}"'
    adb_bin_path = os.path.join(_SRC_PATH, 'third_party', 'android_sdk',
                                'public', 'platform-tools', 'adb')
    adb_bin_path_arg_fragment = f',"adb_bin":"{adb_bin_path}"'
    disable_features = [
        'SpareRendererForSitePerProcess',
        'AndroidWarmUpSpareRendererWithTimeout',
        'WebViewPrefetchNativeLibrary',
    ]

    cmd = [
        'tools/perf/cb',
        'embedder',
        f'--browser={{browser:"clank/android_webview/tools/crossbench_config/cipd/{arch}/Velvet_{arch}.apk",driver:{{type:"Android"'
        + adb_bin_path_arg_fragment + '}}',
    ] + driver_path_arg + [
        '--splashscreen=skip',
        '--cuj-config=third_party/crossbench/config/team/woa/embedder_cuj_config.hjson',
        '--network={"type":"wpr","path":"tools/perf/page_sets/data/crossbench_android_embedder_000.wprgo"'
        + wpr_bin_path_arg_fragment +
        ',"skip_deterministic_script_injection":true}',
        '--embedder-process-name=googleapp',
        '--embedder-setup-command-config=clank/android_webview/tools/crossbench_config/agsa_setup_config.hjson',
        f'--disable-features={",".join(disable_features)}',
    ]
    subprocess.check_call(cmd, cwd=_SRC_PATH)


def launch_gma():
    # TODO(ziadyoussef): Use Crossbench instead and set command line args.
    _LOGGER.info("Launching Mobile Ads WebView (Interstitial)...")
    cmd = [
        'adb', 'shell', 'am', 'start', '-n',
        'com.google.android.libraries.ads.mobile.maitier.testapps.webview/'
        '.MainActivity', '--ez', 'auto_show_interstitial', 'true'
    ]
    subprocess.check_call(cmd)


def pull_profraw(device, device_profiles_dir, profiledir):
    files = device.ListDirectory(device_profiles_dir)
    _LOGGER.info(f"Pulling {len(files)} profiles from {device_profiles_dir}:")
    for f in files:
        _LOGGER.info(f"  {f}")

    device.PullFile(device_profiles_dir, profiledir)


def run_target(device, target: str, args: OptionsNamespace):
    if target == 'agsa':
        package = 'com.google.android.googlequicksearchbox'
        process_name = f'{package}:googleapp'
        launch_func = lambda: launch_agsa(args)
    else:
        package = (
            'com.google.android.libraries.ads.mobile.maitier.testapps.webview')
        process_name = package
        launch_func = launch_gma

    device_profiles_dir = f'/data/user/0/{package}/cache/pgo_profiles/'

    # Clean up intermediate files from previous runs.
    profraw_path = f'{args.profiledir}/{target}/raw'
    _LOGGER.debug(f"Raw profile path: {profraw_path}")

    if os.path.exists(profraw_path):
        _LOGGER.debug(
            f"Removing existing raw profile directory: {profraw_path}")
        shutil.rmtree(profraw_path)
    os.makedirs(profraw_path, exist_ok=True)

    profdata_path = f'{args.profiledir}/{target}.profdata'
    _LOGGER.debug(f"profdata path: {profdata_path}")
    if os.path.exists(profdata_path):
        _LOGGER.debug(f"Removing existing profdata file: {profdata_path}")
        os.remove(profdata_path)

    _LOGGER.info(f"Running benchmark for {target}...")
    device.ForceStop(package)
    clear_remote_profiles(device, device_profiles_dir)
    launch_func()

    _LOGGER.info(f"Waiting 10 seconds for {target} page load...")
    time.sleep(10)

    pid = get_pid(device, process_name)
    if not pid:
        _LOGGER.error(f"Could not find PID for {process_name}")
        return

    _LOGGER.info(f"Found PID {pid} for {process_name}")
    forward_socket(device, pid)
    try:
        if trigger_dump():
            # Wait a moment for the device to finish writing files
            time.sleep(10)
            pull_profraw(device, device_profiles_dir, profraw_path)

            if not args.skip_profdata:
                profraw_files = glob.glob(f'{profraw_path}/**/*.profraw',
                                          recursive=True)
                if not profraw_files:
                    _LOGGER.error(f"No profraw files found in {profraw_path}")
                    return

                run_profdata_merge(profdata_path, profraw_files, args)

                # Test merge to prevent issues like: https://crbug.com/353702041
                with tempfile.NamedTemporaryFile() as f:
                    _LOGGER.debug("Testing profdata merge")
                    run_profdata_merge(f.name, [profdata_path], args)
    finally:
        device.adb.ForwardRemove('tcp:9222')

    device.ForceStop(package)


def run_webview_benchmarks(device, args: OptionsNamespace):
    # The method should:
    # 1- Run the benchmarks
    # 2- Trigger the PGO dump
    # 3- Pull profdata into {profiledir}
    if args.profile_target == 'agsa':
        targets = ['agsa']
    elif args.profile_target == 'gma':
        targets = ['gma']
    elif args.profile_target == 'combined':
        targets = ['agsa', 'gma']
    else:
        raise ValueError(f'Unsupported profile target: {args.profile_target}')

    for target in targets:
        run_target(device, target, args)


def main():
    args = parse_args()

    devices = device_utils.DeviceUtils.HealthyDevices()
    if not devices:
        _LOGGER.error('No healthy devices found')
        return 1
    device = devices[0]
    device.EnableRoot()

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

    os.makedirs(args.profiledir)

    install_and_set_provider(args)
    run_webview_benchmarks(device, args)

    if not args.skip_profdata:
        # Bots run a separate merge step (merge_results.py) that expects profraw
        # files instead of profdata files.
        suffix = ".profraw" if args.isolated_script_test_output else ".profdata"
        profile_output_path = f'{args.outputdir}/profile{suffix}'
        merge_profdata(profile_output_path, args)

    if not args.keep_temps:
        _LOGGER.info('Cleaning up %s, use --keep-temps to keep it.',
                     args.profiledir)
        shutil.rmtree(args.profiledir, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

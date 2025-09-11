#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script that runs multiple benchmarks on multiple emulators.

This script assumes that it will be run from the source dir (i.e. //). It is
best to run this on a new repository created from scratch and removed after
each time.
"""

import argparse
import dataclasses
import itertools
from pprint import pformat
import random
import time
import subprocess
import sys
from typing import Optional

_START_TIME = time.time()
_QUIET = False


def _log(message: str):
    if not _QUIET:
        print(f"timeall.py-{time.time() - _START_TIME:.0f}s: {message}",
              file=sys.stderr)


def _run_command(command: list[str]) -> str:
    _log(f'Running: {" ".join(command)}')
    process = subprocess.run(command,
                             capture_output=True,
                             text=True,
                             check=True)
    _log(process.stderr)
    return process.stdout


def _run_command_with_repeat(command: list[str], repeat: int,
                            outdir_name: str) -> Optional[str]:
    _log(f"Running with {repeat=}")
    for idx in range(repeat):
        try:
            return _run_command(command)
        except subprocess.CalledProcessError as e:
            _log("Stdout:")
            _log(e.stdout)
            _log("Stderr:")
            _log(e.stderr)
            if idx != repeat - 1:
                _log(f"Repeating... {idx + 1}/{repeat}")
                _run_command(["gn", "clean", outdir_name])
    return None


@dataclasses.dataclass(frozen=True)
class _Options:
    benchmark: str
    r: int # times to repeat the benchmark.
    i: bool = False  # incremental_install
    n: bool = False  # no_component_build
    s: bool = False  # server
    e: str = ""  # emulator


def _run_benchmark(options: _Options):
    outdir_name = f"out/Debug"
    if '_test_' in options.benchmark:
        target = "chrome_test_apk"
    else:
        target = "chrome_apk"
    cmd = [
        "tools/android/build_speed/benchmark.py",
        "-vv",
        options.benchmark,
        "-C",
        outdir_name,
        "--target",
        target,
    ]
    if options.e:
        cmd.extend(["--emulator", options.e])
    else:
        # Default to 64-bit.
        cmd.append("--build-64bit")
    if not options.i:
        cmd.append("--no-incremental-install")
    if options.n:
        cmd.append("--no-component-build")
    if not options.s:
        cmd.append("--no-server")
    _log(f"Start {options=}")
    output = _run_command_with_repeat(cmd,
                                     repeat=options.r,
                                     outdir_name=outdir_name)
    assert output is not None
    if not _QUIET:
        print(output, end="", flush=True)
    _log(f"Done {options=}")


def _run_benchmarks(bos: list[_Options], **kwargs):
    for o in bos:
        _run_benchmark(dataclasses.replace(o, **kwargs))


def run(debug: bool):
    benchmarks = [
        "module_internal_nosig",
        "chrome_nosig",
        "base_sig",
        "cta_test_sig",
    ]

    if debug:
        benchmarks = [benchmarks[0]]
    else:
        random.shuffle(benchmarks)

    emulators = [  # base options, always do api34.
        "android_34_google_apis_x64_local.textpb",
    ]

    if not debug:
        additional_emulators = [
            "android_31_google_apis_x64_local.textpb",
            "android_33_google_apis_x64_local.textpb",
            "android_35_google_apis_x64_local.textpb",
            "android_36_google_apis_x64_local.textpb",
        ]
        # Only compare two emulator each time.
        emulators += [random.choice(additional_emulators)]
        random.shuffle(emulators)

    if debug:
        repeat = 1
    else:
        repeat = 3

    bos = [
        _Options(benchmark=benchmark, r=repeat, e=emulator, i=i, n=n, s=s)
        for benchmark, emulator, i, n, s in itertools.product(
            benchmarks, emulators, [True, False], [True, False], [True, False])
    ]

    # shuffle bos
    if debug:
        bos = [bos[0]]
    else:
        random.shuffle(bos)

    _log(pformat(bos))

    _run_benchmarks(bos=bos)


def main():
    argparser = argparse.ArgumentParser()
    argparser.add_argument("--debug", action="store_true")
    argparser.add_argument("-q", "--quiet", action="store_true")
    args = argparser.parse_args()
    global _QUIET
    _QUIET = args.quiet
    run(args.debug)


if __name__ == "__main__":
    main()

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

start_time = time.time()

DEBUG = False


def _log(message: str):
    print(f"timeall.py-{time.time() - start_time:.0f}s: {message}",
          file=sys.stderr)


def run_command(command: list[str]) -> str:
    _log(f'Running: {" ".join(command)}')
    process = subprocess.run(command,
                             capture_output=True,
                             text=True,
                             check=True)
    _log(process.stderr)
    return process.stdout


def run_command_with_repeat(command: list[str], repeat: int,
                            outdir_name: str) -> Optional[str]:
    _log(f"Running with {repeat=}")
    for idx in range(repeat):
        try:
            return run_command(command)
        except subprocess.CalledProcessError as e:
            _log("Stdout:")
            _log(e.stdout)
            _log("Stderr:")
            _log(e.stderr)
            if idx != repeat - 1:
                _log(f"Repeating... {idx + 1}/{repeat}")
                run_command(["gn", "clean", outdir_name])
    return None


@dataclasses.dataclass(frozen=True)
class Options:
    benchmark: str = "module_java_internal_nosig"
    i: bool = False  # incremental_install
    n: bool = False  # no_component_build
    s: bool = False  # server
    e: str = ""  # emulator


def run_benchmark(options: Options):
    outdir_name = f"out/Debug"
    cmd = [
        "tools/android/build_speed/benchmark.py",
        "-vv",
        options.benchmark,
        "-C",
        outdir_name,
        "--target",
        "chrome_apk",
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
    output = run_command_with_repeat(cmd,
                                     repeat=3 if not DEBUG else 1,
                                     outdir_name=outdir_name)
    assert output is not None
    print(output, end="", flush=True)
    _log(f"Done {options=}")


def run_benchmarks(bos: list[Options], **kwargs):
    for o in bos:
        run_benchmark(dataclasses.replace(o, **kwargs))


def main():
    global DEBUG
    argparser = argparse.ArgumentParser()
    argparser.add_argument("--debug", action="store_true")
    args = argparser.parse_args()

    DEBUG = args.debug

    benchmarks = [
        "chrome_nosig",
        "base_sig",
        "module_internal_nosig",
    ]

    if args.debug:
        benchmarks = [benchmarks[0]]
    else:
        random.shuffle(benchmarks)
        # Only compare two benchmarks each time.
        benchmarks = benchmarks[:2]

    emulators = [  # base options
        "android_31_google_apis_x64_local.textpb",
        "android_33_google_apis_x64_local.textpb",
        "android_34_google_apis_x64_local.textpb",
    ]

    if args.debug:
        emulators = [emulators[0]]
    else:
        random.shuffle(emulators)
        # Only compare two emulator each time.
        emulators = emulators[:2]

    bos = [
        Options(benchmark=benchmark, e=emulator, i=i, n=n, s=s)
        for benchmark, emulator, i, n, s in itertools.product(
            benchmarks, emulators, [True, False], [True, False], [True, False])
    ]

    # shuffle bos
    if not args.debug:
        random.shuffle(bos)

    _log(pformat(bos))

    run_benchmarks(bos=bos)


if __name__ == "__main__":
    main()

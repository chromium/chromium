#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rewrites Chromium across multiple platforms using the spanify tool."""

import argparse
import contextlib
import logging
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import time

from gnconfigs import GnConfigs
from spanify_utils import scratch_dir, clear_scratch_dir

PROJECTS = {
    'chrome': {
        'compile_dirs': '.',
        'tool_arg': '',
        'build_targets': ['//:gn_all'],
    },
    'partition_alloc': {
        'compile_dirs':
        'base/allocator/partition_allocator/src',
        'tool_arg':
        '--project=partition_alloc',
        'build_targets':
        ['//base/allocator/partition_allocator/src/partition_alloc'],
    },
    'skia': {
        'compile_dirs': 'third_party/skia',
        'tool_arg': '--project=skia',
        'build_targets': ['//third_party/skia']
    },
    'dawn': {
        'compile_dirs': 'third_party/dawn/src',
        'tool_arg': '--project=dawn',
        'build_targets': ['//third_party/dawn/src/dawn'],
    },
    'webrtc': {
        'compile_dirs':
        'third_party/webrtc',
        'tool_arg':
        '--project=webrtc',
        'build_targets': [
            '//third_party/webrtc_overrides:webrtc_component',

            # These proto targets are needed to build some required generated
            # files that the script isn't smart enough to discover on its own.
            '//third_party/webrtc/api/test/metrics:metric_proto',
            '//third_party/catapult/tracing/tracing/proto:histogram_proto',
            '//third_party/webrtc/api/test/network_emulation:network_config_schedule_proto',
            '//third_party/webrtc/modules/audio_coding:neteq_unittest_proto',
        ],
    },
}

# Standard GN arguments common to most platforms for spanification.
COMMON_EXTRA_GN_ARGS = [
    'clang_use_chrome_plugins = false',
    'force_enable_raw_ptr_exclusion = true',
]

# Discover all available platforms and configurations from gnconfigs.
AVAILABLE_PLATFORMS = sorted(set(GnConfigs(False).min_all_platforms.keys()) |
                             set(GnConfigs(False).all_platforms_and_configs.keys()))

LLVM_BUILD_DIR = pathlib.Path('third_party/llvm-build')
LLVM_UPSTREAM_DIR = pathlib.Path('third_party/llvm-build-upstream')
REWRITER_BINARY_PATH = LLVM_BUILD_DIR / 'Release+Asserts/bin/spanify'
CLANG_LIB_REL_PATH = pathlib.Path('Release+Asserts/lib/clang')


def get_out_dir(platform):
    """
    Standardizes the output directory path for a given platform.
    """
    return pathlib.Path(f'out/rewrite-{platform}')


def get_platform_args(platform):
    """
    Returns the GN arguments for the given platform.
    """
    configs = GnConfigs(False)

    args = configs.min_all_platforms.get(platform)
    if args is None:
        # If not in min_all_platforms, try to get it directly from all configs.
        args = configs[platform]

    if args is None:
        raise ValueError(f"Unknown platform: {platform}")

    return '\n'.join(args + COMMON_EXTRA_GN_ARGS) + '\n'


@contextlib.contextmanager
def build_and_manage_llvm():
    """
    Manages the llvm-build directory state and builds the rewriter.

    There are two main workflows:
    1. Workflow A (Temporary): The user does NOT have a custom spanify build.
       This script backs up the current llvm-build, builds spanify from scratch,
       yields to run the rewrite, and then restores the original llvm-build.
    2. Workflow B (Custom): The user ALREADY has a custom spanify build.
       The script does an incremental build via ninja and leaves the
       llvm-build directory as is.

    To build the spanify tool separately and keep it persistently, run:
    $ tools/clang/scripts/build.py --with-android --without-fuchsia \
        --with-ml-inliner-model= --extra-tools spanify
    """
    is_incremental = REWRITER_BINARY_PATH.exists()
    backed_up = False

    # Backup logic: only backup if we have an llvm-build and it's not already
    # a custom one (i.e., it doesn't have the rewriter tool).
    if not LLVM_UPSTREAM_DIR.exists() and LLVM_BUILD_DIR.exists():
        if not is_incremental:
            logging.info('Saving current llvm-build to %s', LLVM_UPSTREAM_DIR)
            shutil.move(LLVM_BUILD_DIR, LLVM_UPSTREAM_DIR)
            backed_up = True
        else:
            logging.info(
                'llvm-build already contains rewriter, skipping backup.')

    if is_incremental:
        logging.info('Building the rewriter incrementally...')
        run_command(['ninja', '-C', str(LLVM_BUILD_DIR / 'Release+Asserts')])
    else:
        logging.info('Building the rewriter completely...')
        run_command([
            'tools/clang/scripts/build.py',
            '--with-android',
            '--without-fuchsia',
            '--with-ml-inliner-model=',
            '--extra-tools',
            'spanify',
        ])

    try:
        yield
    finally:
        if backed_up:
            logging.info('Restoring llvm-build from %s', LLVM_UPSTREAM_DIR)
            shutil.rmtree(LLVM_BUILD_DIR, ignore_errors=True)
            if LLVM_UPSTREAM_DIR.exists():
                shutil.move(LLVM_UPSTREAM_DIR, LLVM_BUILD_DIR)
        elif is_incremental:
            logging.warning('Keeping customized llvm-build. '
                            'Regular builds will be slower until restored.')


def run_command(cmd, **kwargs):
    """
    Logs and runs a command.
    """
    logging.info('Running: %s', shlex.join(cmd))
    start = time.time()
    subprocess.check_call(cmd, **kwargs)
    logging.info('Done in %.2fs', time.time() - start)


def prepare_platform(platform, out_dir, project):
    """
    Generates args.gn and builds necessary generated files for the given
    platform.
    """
    compile_dirs = PROJECTS[project]['compile_dirs']

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / 'args.gn').write_text(get_platform_args(platform))

    # Mac requires specific runtime libraries from the upstream build.
    if platform == 'mac' and LLVM_UPSTREAM_DIR.exists():
        for v_dir in (LLVM_UPSTREAM_DIR / CLANG_LIB_REL_PATH).iterdir():
            if not v_dir.is_dir():
                continue
            src = v_dir / 'lib/darwin'
            if src.exists():
                dst = (LLVM_BUILD_DIR / CLANG_LIB_REL_PATH / v_dir.name /
                       'lib/darwin')
                shutil.copytree(src, dst, dirs_exist_ok=True)

    logging.info('Generating build files for %s...', platform)
    run_command(['gn', 'gen', str(out_dir)])

    # Build generated files that a successful compilation depends on.
    logging.info('Building generated targets for %s...', platform)

    targets_proc = subprocess.Popen(
        ['ninja', '-C', str(out_dir), '-t', 'targets', 'all'],
        stdout=subprocess.PIPE,
        text=True)
    assert targets_proc.stdout is not None

    targets = []
    # If compile_dirs is '.', we want to match anything in gen/
    # Otherwise, we want to match anything in gen/compile_dirs/
    if compile_dirs == '.':
        pattern = re.compile(r'^gen/.*(\.h|inc|css_tokenizer_codepoints\.cc)$')
    else:
        escaped_path = re.escape(compile_dirs.strip('/'))
        pattern = re.compile(
            rf'^gen/{escaped_path}/.*(\.h|inc|css_tokenizer_codepoints\.cc)$')

    for line in targets_proc.stdout:
        # Ninja targets output format is "target: rule"
        target = line.split(':')[0].strip()
        if pattern.match(target):
            targets.append(target)

    targets_proc.wait()

    if not targets:
        logging.info('No generated targets to build for %s.', platform)
        return

    logging.info('Total generated targets to build for %s: %d', platform,
                 len(targets))
    for t in targets:
        logging.info('-  %s', t)

    # Generate them in batches to avoid hitting command line length limits. The
    # xargs -s option allows us to specify the maximum command line length.
    # We use a fraction of the system's ARG_MAX limit, capped at 1MB, to be safe
    # against environment variable size fluctuations.
    arg_max = min(os.sysconf('SC_ARG_MAX') // 2, 1000000)
    subprocess.run(['xargs', '-s',
                    str(arg_max), 'ninja', '-C',
                    str(out_dir)],
                   input='\n'.join(targets),
                   text=True,
                   check=True)


def run_rewrite_tool(platform, out_dir, project):
    """
    Runs the spanify tool and filters output for uniqueness using awk.
    """
    compile_dirs = PROJECTS[project]['compile_dirs']
    tool_arg = PROJECTS[project]['tool_arg']

    logging.info('Starting rewrite for %s...', platform)

    cmd = [
        'tools/clang/scripts/run_tool.py',
        '--tool',
        'spanify',
        '--generate-compdb',
        '-p',
        str(out_dir),
    ]
    # Optional flags MUST be inserted before positional arguments
    if platform == 'win':
        cmd.append('--target_os=win')
    if tool_arg:
        cmd.append(f'--tool-arg={tool_arg}')

    cmd.extend([compile_dirs, f'path-filter={compile_dirs}'])

    out_path = scratch_dir() / f'rewriter-{platform}.main.out'
    err_path = scratch_dir() / f'rewriter-{platform}.main.err'

    logging.info("Running: %s | awk '!x[$0]++' > %s", shlex.join(cmd),
                 out_path)

    with out_path.open('w') as out_f, err_path.open('w') as err_f:
        # Run run_tool.py and pipe it to awk
        proc1 = subprocess.Popen(cmd,
                                 stdout=subprocess.PIPE,
                                 stderr=err_f,
                                 text=True)
        proc2 = subprocess.Popen(['awk', '!x[$0]++'],
                                 stdin=proc1.stdout,
                                 stdout=out_f,
                                 text=True)

        # Allow proc1 to receive SIGPIPE if awk exits early
        assert proc1.stdout is not None
        proc1.stdout.close()

        proc2.wait()
        proc1.wait()

        if proc1.returncode != 0:
            logging.warning(
                'Rewrite tool failed for %s with exit code %d. '
                'Continuing with partial results.', platform, proc1.returncode)


def run_rewrite_phase(platforms, project):
    """
    Prepares and runs the rewrite tool for all specified platforms.
    """
    clear_scratch_dir()

    for platform in platforms:
        out_dir = get_out_dir(platform)
        prepare_platform(platform, out_dir, project)
        run_rewrite_tool(platform, out_dir, project)


def apply_edits_phase(last_platform):
    """
    Extracts edits from the rewrite tool output and applies them.
    """
    scratch_dir().mkdir(parents=True, exist_ok=True)
    logging.info('Applying edits...')
    # Clear out stale patches from previous runs or tests. They will be
    # recreated by apply_edits.py, but we want to make sure we don't have any
    # leftover patches lying around from previous runs that could cause
    # confusion.
    for f in scratch_dir().glob('patch*'):
        f.unlink()

    logging.info('Running: extract_edits.py | apply_edits.py')

    # Combine all rewrite outputs into one file first
    all_edits = scratch_dir() / 'all.main.out'
    with all_edits.open('wb') as wfd:
        # Use sorted to guarantee reproducible ordering across environments
        for f in sorted(scratch_dir().glob('*.main.out')):
            if f != all_edits:
                with f.open('rb') as rfd:
                    shutil.copyfileobj(rfd, wfd)

    # Let the OS pipe them naturally
    with all_edits.open('rb') as rfd:
        extract_proc = subprocess.Popen(
            ['tools/clang/spanify/extract_edits.py'],
            stdin=rfd,
            stdout=subprocess.PIPE)

        assert extract_proc.stdout is not None
        apply_proc = subprocess.Popen([
            'tools/clang/scripts/apply_edits.py', '-p',
            str(get_out_dir(last_platform)), '.'
        ],
                                      stdin=extract_proc.stdout)

        # Allow extract to receive SIGPIPE if apply exits early
        extract_proc.stdout.close()
        apply_proc.communicate()

    if extract_proc.wait() != 0:
        logging.warning('extract_edits.py failed with exit code %d.',
                        extract_proc.returncode)
    if apply_proc.returncode != 0:
        logging.warning('apply_edits.py failed with exit code %d.',
                        apply_proc.returncode)


def main():
    parser = argparse.ArgumentParser(
        description='Multi-platform spanify tool.',
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-p',
                        '--platforms',
                        nargs='+',
                        choices=AVAILABLE_PLATFORMS,
                        default=['linux-rel'],
                        metavar='PLATFORM',
                        help='Platforms to rewrite. Available options:\n  ' +
                        '\n  '.join(AVAILABLE_PLATFORMS) +
                        '\n(default: linux-rel)')
    parser.add_argument('-r', '--skip-rewrite', action='store_true')
    parser.add_argument('-e', '--skip-extract-edits', action='store_true')
    parser.add_argument('--project', choices=PROJECTS.keys(), default='chrome')
    args = parser.parse_args()

    # Configure logging.
    logging.basicConfig(level=logging.DEBUG,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')

    with build_and_manage_llvm():
        logging.info('Testing rewriter...')
        run_command(['tools/clang/spanify/run_all_tests.py'])

        if not args.skip_rewrite:
            run_rewrite_phase(args.platforms, args.project)

        if not args.skip_extract_edits:
            apply_edits_phase(args.platforms[-1])

        logging.info('Formatting...')
        run_command(['git', 'cl', 'format'])


if __name__ == '__main__':
    main()

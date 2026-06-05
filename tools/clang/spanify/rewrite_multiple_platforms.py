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
import sys
import time

from gnconfigs import GnConfigs
from project import PROJECTS
from spanify_utils import scratch_dir, clear_scratch_dir


# Standard GN arguments common to most platforms for spanification.
ROOT_DIR = pathlib.Path(__file__).resolve().parents[3]
CLANG_BASE_PATH = ROOT_DIR / 'third_party/llvm-build/Release+Asserts'
GN_PATH = ROOT_DIR / 'buildtools/linux64/gn'

COMMON_EXTRA_GN_ARGS = [
    'clang_use_chrome_plugins = false',
    'force_enable_raw_ptr_exclusion = true',
    f'clang_base_path = "{CLANG_BASE_PATH}"',
]


# Discover all available platforms and configurations from gnconfigs.
AVAILABLE_PLATFORMS = sorted(set(GnConfigs(False).min_all_platforms.keys()) |
                             set(GnConfigs(False).all_platforms_and_configs.keys()))

LLVM_BUILD_DIR = pathlib.Path('third_party/llvm-build')
LLVM_UPSTREAM_DIR = pathlib.Path('third_party/llvm-build-upstream')
REWRITER_BINARY_PATH = LLVM_BUILD_DIR / 'Release+Asserts/bin/spanify'
CLANG_LIB_REL_PATH = pathlib.Path('Release+Asserts/lib/clang')


def get_out_dir(platform, submodule='.'):
    """
    Standardizes the output directory path for a given platform.
    """
    return pathlib.Path(submodule) / 'out' / f'rewrite-{platform}'


def get_platform_args(platform, project='chrome'):
    """
    Returns the GN arguments for the given platform.
    """
    configs = GnConfigs(False)

    args = configs.get_config(platform, project)

    if args is None:
        raise ValueError(f"Unknown platform/project combination: "
                         f"{platform}/{project}")

    submodule = PROJECTS[project].get('submodule', '.')
    standalone = submodule != '.'
    extra_args = COMMON_EXTRA_GN_ARGS if not standalone else []

    return '\n'.join(args + extra_args) + '\n'



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


def _prepare_skia(submodule):
    submodule_path = pathlib.Path(submodule)
    gn_bin = submodule_path / 'bin/gn'
    if gn_bin.is_symlink():
        logging.info('Removing stale GN symlink in Skia...')
        gn_bin.unlink()

    # Sync dependencies (which also downloads real bin/gn natively).
    run_command([sys.executable, 'tools/git-sync-deps'], cwd=submodule)


def _prepare_dawn(submodule):
    # Manually write .gclient to bypass depot_tools CLI validation changes.
    submodule_path = pathlib.Path(submodule)
    gclient_content = """solutions = [
  {
    "name": ".",
    "url": "https://dawn.googlesource.com/dawn.git",
    "deps_file": "DEPS",
    "managed": False,
    "custom_deps": {},
  },
]
"""
    (submodule_path / '.gclient').write_text(gclient_content)

    # Sync dependencies.
    run_command(['gclient', 'sync', '--no-history', '--shallow'],
                cwd=submodule)


def _prepare_angle(submodule):
    # Bootstrap (generates .gclient).
    run_command([sys.executable, 'scripts/bootstrap.py'], cwd=submodule)

    # Sync dependencies.
    run_command(['gclient', 'sync', '--no-history', '--shallow'],
                cwd=submodule)

    # Patch gclient_args.gni.
    submodule_path = pathlib.Path(submodule)
    gclient_args_path = submodule_path / 'build/config/gclient_args.gni'
    if gclient_args_path.exists():
        content = gclient_args_path.read_text()
        if 'checkout_src_internal' not in content:
            logging.info('Patching ANGLE gclient_args.gni...')
            with gclient_args_path.open('a') as f:
                f.write('\ncheckout_src_internal = false\n')


def _prepare_webrtc(submodule):
    # WebRTC expects to be checked out inside a "src" solution directory.
    # We write the .gclient file to the parent directory and sync there.
    submodule_path = pathlib.Path(submodule)
    parent_path = submodule_path.parent
    gclient_content = """solutions = [
  {
    "name": "src",
    "url": "https://webrtc.googlesource.com/src.git",
    "deps_file": "DEPS",
    "managed": False,
    "custom_deps": {},
  },
]
"""
    (parent_path / '.gclient').write_text(gclient_content)

    # Sync dependencies.
    run_command(['gclient', 'sync', '--no-history', '--shallow'],
                cwd=parent_path)


def prepare_standalone_project(submodule, project):
    """
    Syncs dependencies and applies setup fixes for a standalone project.
    """
    logging.info('Preparing standalone project %s in %s...', project,
                 submodule)
    match project:
        case 'skia':
            _prepare_skia(submodule)
        case 'dawn':
            _prepare_dawn(submodule)
        case 'angle':
            _prepare_angle(submodule)
        case 'webrtc':
            _prepare_webrtc(submodule)


def prepare_platform(platform, out_dir, project):
    """
    Generates args.gn and builds necessary generated files for the given
    platform.
    """
    submodule = PROJECTS[project].get('submodule', '.')
    compile_dirs = PROJECTS[project]['compile_dirs']
    standalone = submodule != '.'

    if standalone:
        prepare_standalone_project(submodule, project)

    out_dir.mkdir(parents=True, exist_ok=True)


    # Generate build files.
    platform_args = get_platform_args(platform, project)
    (out_dir / 'args.gn').write_text(platform_args)

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

    logging.info('Generating build files for %s in %s...', platform, submodule)
    out_dir_arg = out_dir.relative_to(submodule) if standalone else out_dir
    run_command([str(GN_PATH), 'gen', str(out_dir_arg)],
                cwd=submodule if standalone else None)


    # Build generated files that a successful compilation depends on.
    logging.info('Building generated targets for %s...', platform)

    targets_proc = subprocess.Popen(
        ['ninja', '-C',
         str(out_dir_arg), '-t', 'targets', 'all'],
        stdout=subprocess.PIPE,
        text=True,
        cwd=submodule if standalone else None)

    assert targets_proc.stdout is not None

    targets = []
    # If compile_dirs is '.', we want to match anything in gen/
    # Otherwise, we want to match anything in gen/compile_dirs/
    if compile_dirs == '.' or standalone:
        # In standalone, compile_dirs is usually relative to submodule root.
        # But generated files are usually in gen/
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

    # Generate them in batches to avoid hitting command line length limits.
    arg_max = min(os.sysconf('SC_ARG_MAX') // 2, 1000000)
    subprocess.run(
        ['xargs', '-s',
         str(arg_max), 'autoninja', '-C',
         str(out_dir_arg)],
        input='\n'.join(targets),
        text=True,
        check=True,
        cwd=submodule if standalone else None)




def run_rewrite_tool(platform, out_dir, project):
    """
    Runs the spanify tool and filters output for uniqueness using awk.
    """
    submodule = PROJECTS[project].get('submodule', '.')
    compile_dirs = PROJECTS[project]['compile_dirs']
    tool_arg = f'--project={project}'
    standalone = submodule != '.'

    logging.info('Starting rewrite for %s in %s...', platform, submodule)

    root_dir = pathlib.Path(os.getcwd())
    run_tool_path = root_dir / 'tools/clang/scripts/run_tool.py'

    if standalone:
        out_dir_arg = out_dir.relative_to(submodule)
    else:
        out_dir_arg = out_dir

    cmd = [
        sys.executable,
        str(run_tool_path),
        '--tool',
        'spanify',
        '--generate-compdb',
        '-p',
        str(out_dir_arg),
    ]
    # Optional flags MUST be inserted before positional arguments
    if platform == 'win':
        cmd.append('--target_os=win')
    if tool_arg:
        cmd.append(f'--tool-arg={tool_arg}')
    # Standard LLVM Clang compilation flags are required to prevent custom
    # Chromium pragmas (like #pragma allow_unsafe_buffers) from being treated
    # as fatal unknown pragma errors during submodule rewrites.
    cmd.append('--tool-arg=-extra-arg=-Wno-error')
    cmd.append('--tool-arg=-extra-arg=-Wno-unknown-pragmas')

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
                                 text=True,
                                 cwd=submodule if standalone else None)
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


def apply_edits_phase(platform, out_dir, project):
    """
    Collects all rewriter output and applies edits using apply_edits.py.
    """
    submodule = PROJECTS[project].get('submodule', '.')
    standalone = submodule != '.'

    logging.info('Applying edits...')
    all_edits = scratch_dir() / 'all_edits.out'
    with all_edits.open('wb') as wfd:
        for f in sorted(scratch_dir().glob('*.main.out')):
            if f != all_edits:
                with f.open('rb') as rfd:
                    shutil.copyfileobj(rfd, wfd)

    root_dir = pathlib.Path(os.getcwd())
    extract_edits_path = root_dir / 'tools/clang/spanify/extract_edits.py'
    apply_edits_path = root_dir / 'tools/clang/scripts/apply_edits.py'

    if standalone:
        out_dir_arg = out_dir.relative_to(submodule)
    else:
        out_dir_arg = out_dir

    # Let the OS pipe them naturally
    with all_edits.open('rb') as rfd:
        extract_proc = subprocess.Popen(
            [sys.executable, str(extract_edits_path)],
            stdin=rfd,
            stdout=subprocess.PIPE)

        assert extract_proc.stdout is not None
        apply_proc = subprocess.Popen([
            sys.executable,
            str(apply_edits_path),
            '-p',
            str(out_dir_arg),
        ],
                                      stdin=extract_proc.stdout,
                                      cwd=submodule if standalone else None)

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
        description='Rewrite multiple platforms using spanify.')
    parser.add_argument(
        '--platform',
        action='append',
        help='Platforms to rewrite (e.g., linux, win, mac, android). '
        'Can be specified multiple times.')
    parser.add_argument('--project',
                        choices=PROJECTS.keys(),
                        default='chrome',
                        help='Project to rewrite.')

    args = parser.parse_args()

    # Set CHROMIUM_BUILDTOOLS_PATH globally so LLVM/Clang tools find the
    # correct buildtools directory when executed inside submodule roots.
    os.environ['CHROMIUM_BUILDTOOLS_PATH'] = str(ROOT_DIR / 'buildtools')

    platforms = args.platform or ['linux']
    project = args.project
    submodule = PROJECTS[project].get('submodule', '.')

    logging.basicConfig(level=logging.INFO,
                        format='[%(asctime)s %(levelname)s] %(message)s',
                        datefmt='%H:%M:%S')

    clear_scratch_dir()

    with build_and_manage_llvm():
        for platform in platforms:
            out_dir = get_out_dir(platform, submodule)
            prepare_platform(platform, out_dir, project)
            run_rewrite_tool(platform, out_dir, project)

        # Apply edits after running the tool on all requested platforms.
        # We use the last platform's out_dir for apply_edits.py as it needs
        # a valid build directory to find headers, etc.
        apply_edits_phase(platforms[-1], get_out_dir(platforms[-1], submodule),
                          project)

    logging.info('Formatting...')
    # Running git cl format in the submodule if necessary.
    try:
        run_command(['git', 'cl', 'format'], cwd=submodule)
    except subprocess.CalledProcessError:
        logging.warning('git cl format failed, continuing...')



if __name__ == '__main__':
    main()

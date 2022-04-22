#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import logging
import multiprocessing
import os
import pathlib
import subprocess
import sys
from typing import List, Optional, Tuple

import json_gn_editor
import utils

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).parents[2].resolve()
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils, subprocess_utils

_SRC_PATH = git_metadata_utils.get_chromium_src_path()
sys.path.append(str(_SRC_PATH / 'build' / 'android'))
from pylib import constants

_GIT_IGNORE_STR = '(git ignored file) '


@dataclasses.dataclass
class OperationResult:
    path: str
    ignored: bool
    dryrun: bool

    def __str__(self):
        dryrun_string = '[DRYRUN] ' if self.dryrun else ''
        ignore_string = _GIT_IGNORE_STR if self.ignored else ''
        return f'{dryrun_string}Updated {ignore_string}{self.path}'


def _split_deps(existing_dep: str, new_deps: List[str], root: pathlib.Path,
                path: str, dryrun: bool) -> Optional[OperationResult]:
    with json_gn_editor.BuildFile(path, root, dryrun=dryrun) as build_file:
        if build_file.split_deps(existing_dep, new_deps):
            return OperationResult(path=os.path.relpath(path, start=root),
                                   ignored=utils.is_git_ignored(root, path),
                                   dryrun=dryrun)
    return None


def _remove_deps(deps: List[str], out_dir: str, root: pathlib.Path, path: str,
                 dryrun: bool, targets: List[str],
                 inline_mode: bool) -> Optional[OperationResult]:
    with json_gn_editor.BuildFile(path, root, dryrun=dryrun) as build_file:
        if build_file.remove_deps(deps, out_dir, targets, inline_mode):
            return OperationResult(path=os.path.relpath(path, start=root),
                                   ignored=utils.is_git_ignored(root, path),
                                   dryrun=dryrun)
    return None


def _split(args: argparse.Namespace, build_filepaths: List[str],
           root: pathlib.Path) -> List[OperationResult]:
    num_total = len(build_filepaths)
    results = []
    with multiprocessing.Pool() as pool:
        tasks = {
            filepath: pool.apply_async(
                _split_deps,
                (args.existing, args.new, root, filepath, args.dryrun))
            for filepath in build_filepaths
        }
        for idx, filepath in enumerate(tasks.keys()):
            logging.info('[%d/%d] Checking %s', idx, num_total, filepath)
            operation_result = tasks[filepath].get()
            if operation_result:
                logging.info(operation_result)
                results.append(operation_result)
    return results


def _remove(args: argparse.Namespace, build_filepaths: List[str],
            root: pathlib.Path) -> List[OperationResult]:
    num_total = len(build_filepaths)

    if args.output_directory:
        constants.SetOutputDirectory(args.output_directory)
    constants.CheckOutputDirectory()
    out_dir: str = constants.GetOutDirectory()

    args_gn_path = os.path.join(out_dir, 'args.gn')
    if not os.path.exists(args_gn_path):
        raise Exception(f'No args.gn in out directory {out_dir}')
    with open(args_gn_path) as f:
        # Although the target may compile fine, bytecode checks are necessary
        # for correctness at runtime.
        assert 'android_static_analysis = "on"' in f.read(), (
            'Static analysis must be on to ensure correctness.')
        # TODO: Ensure that the build server is not running.

    logging.info(f'Running gn gen in output directory: {out_dir}')
    subprocess_utils.run_command(['gn', 'gen', '-C', out_dir])
    logging.info('Building targets in preparation for removing deps')
    # Avoid capturing stdout/stderr to see progress of the full build.
    subprocess.run(['autoninja', '-C', out_dir] + args.targets,
                   check=True,
                   stdout=sys.stderr)

    results = []
    for idx, filepath in enumerate(build_filepaths):
        # Since removal can take a long time, provide an easy way to resume the
        # command if something fails.
        try:
            # When resuming, the first build file is the one that is being
            # resumed. Avoid inline mode skipping it since it's already started
            # to be processed and the first dep may already have been removed.
            if args.resume_from and idx == 0 and args.inline_mode:
                logging.info(f'Resuming: skipping inline mode for {filepath}.')
                should_inline = False
            else:
                should_inline = args.inline_mode
            logging.info('[%d/%d] Checking %s', idx, num_total, filepath)
            operation_result = _remove_deps(args.dep, out_dir, root, filepath,
                                            args.dryrun, args.targets,
                                            should_inline)
            if operation_result:
                logging.info(operation_result)
                results.append(operation_result)
        # Use blank except: to show this for KeyboardInterrupt as well.
        except:
            logging.error(
                f'Encountered error while processing {filepath}. Append the '
                'following args to resume from this file once the error is '
                f'fixed:\n\n--resume-from {filepath}\n')
            raise
    return results


def main():
    parser = argparse.ArgumentParser(
        description='Add or remove deps programatically.')

    common_args_parser = argparse.ArgumentParser(add_help=False)
    common_args_parser.add_argument(
        '-n',
        '--dryrun',
        action='store_true',
        help='Show which files would be updated but avoid changing them.')
    common_args_parser.add_argument('-v',
                                    '--verbose',
                                    action='store_true',
                                    help='Used to print ninjalog.')
    common_args_parser.add_argument('-q',
                                    '--quiet',
                                    action='store_true',
                                    help='Used to print less logging.')
    common_args_parser.add_argument(
        '--file', help='Run on a specific build file (debugging).')
    common_args_parser.add_argument(
        '--resume-from',
        help='Skip files before this build file path (debugging).')

    subparsers = parser.add_subparsers(
        help='Use subcommand -h to see full usage.')

    split_parser = subparsers.add_parser(
        'split',
        parents=[common_args_parser],
        help='Split one or more deps from an existing dep.')
    split_parser.add_argument('existing', help='The dep to split from.')
    split_parser.add_argument('new',
                              nargs='+',
                              help='One of the new deps to be added.')
    split_parser.set_defaults(command=_split)

    remove_parser = subparsers.add_parser(
        'remove',
        parents=[common_args_parser],
        help='Remove one or more deps if the build still succeeds. Removing '
        'one dep at a time is recommended.')
    remove_parser.add_argument('dep',
                               nargs='+',
                               help='One of the deps to be removed.')
    remove_parser.add_argument(
        '-C',
        '--output-directory',
        metavar='OUT',
        help='If outdir is not provided, will attempt to guess.')
    remove_parser.add_argument(
        '--targets',
        metavar='T',
        nargs='*',
        default=[],
        help='The set of targets to compile after each dep removal. By default '
        'ninja compiles all targets.')
    remove_parser.add_argument(
        '--inline-mode',
        action='store_true',
        help='Skip the build file if the first dep is not found and removed. '
        'This is especially useful when inlining deps so that a build file '
        'that does not contain the dep being inlined can be skipped. This '
        'mode assumes that the first dep is the one being inlined.')
    remove_parser.set_defaults(command=_remove)

    args = parser.parse_args()
    if args.quiet:
        level = logging.WARNING
    elif args.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)7d %(message)s')

    root = git_metadata_utils.get_chromium_src_path()
    if args.file:
        build_filepaths = [os.path.relpath(args.file, root)]
    else:
        build_filepaths = []
        logging.info('Finding build files under %s', root)
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                filepath = os.path.join(dirpath, filename)
                if filename.endswith(('.gn', '.gni')):
                    build_filepaths.append(filepath)
        build_filepaths.sort()

    if args.resume_from:
        resume_idx = None
        for idx, path in enumerate(build_filepaths):
            if path.endswith(args.resume_from):
                resume_idx = idx
                break
        assert resume_idx is not None, f'Did not find {args.resume_from}.'
        build_filepaths = build_filepaths[resume_idx:]

    filtered_build_filepaths = [
        p for p in build_filepaths if not utils.is_bad_gn_file(p)
    ]
    num_total = len(filtered_build_filepaths)
    assert num_total > 0, 'No valid GN files found.'
    logging.info('Found %d build files.', num_total)

    operation_results: List[OperationResult] = args.command(
        args, filtered_build_filepaths, root)
    ignored_operation_results = [r for r in operation_results if r.ignored]
    num_updated = len(operation_results)
    num_ignored = len(ignored_operation_results)
    print(f'Checked {num_total} and updated {num_updated} build files, '
          f'{num_ignored} of which are ignored by git under {root}')
    if num_ignored:
        print(f'\nThe following {num_ignored} files were ignored by git and '
              'may need separate CLs in their respective repositories:')
        for result in ignored_operation_results:
            print('  ' + result.path)


if __name__ == '__main__':
    main()

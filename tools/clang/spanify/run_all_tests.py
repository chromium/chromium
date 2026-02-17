#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import difflib
import glob
import json
import os
import shutil
import subprocess
import sys

# This script runs the spanify tool tests.


def _RunGit(args):
    command = ['git.bat'] if sys.platform == 'win32' else ['git']
    command.extend(args)
    subprocess.check_call(command)


def _GenerateCompileCommands(files, include_paths):
    """Returns a JSON string containing a compilation database for the input."""
    files = [f.replace('\\', '/') for f in files]
    include_path_flags = ' '.join('-I %s' % include_path.replace('\\', '/')
                                  for include_path in include_paths)
    return json.dumps([{
        'directory':
        os.path.dirname(f),
        'command':
        'clang++ -std=c++23 -fsyntax-only %s %s' %
        (include_path_flags, os.path.basename(f)),
        'file':
        os.path.basename(f)
    } for f in files],
                      indent=2)


def RunScriptsAsPipedCommands(*scripts_cmds):
    processes = []

    python = sys.executable

    prev_stdout = None
    for script_cmd in scripts_cmds:
        processes.append(
            subprocess.Popen(
                [python] + script_cmd,
                stdin=prev_stdout,
                stdout=subprocess.PIPE
            )
        )

        prev_stdout = processes[-1].stdout

    # Wait for the pipeline to finish.
    stdout, _ = processes[-1].communicate()
    for process in processes:
        process.wait()
        if process.returncode != 0:
            print(f'Failure while running: {process.args}')
            return process.returncode

    return 0


def _ApplyTool(scripts_dir, spanify_dir, test_dir, actual_files):
    try:
        # Stage the test files in the git index. If they aren't staged, then
        # run_tool.py will skip them when applying replacements.
        _RunGit(['add'] + actual_files)

        python = sys.executable

        # run_tool.py ... | extract_edits.py | apply_edits.py ...
        returncode = RunScriptsAsPipedCommands(
            # 1. run_tool.py
            [os.path.join(scripts_dir, "run_tool.py"),
                "--tool", "spanify",
                "-p", test_dir
                # Omit --project flag for now as it's not supported by the tool yet.
            ] + actual_files,
            # 2. extract_edits.py (from spanify_dir)
            [os.path.join(spanify_dir, "extract_edits.py")],
            # 3. apply_edits.py
            [os.path.join(scripts_dir, "apply_edits.py"),
                "-p", test_dir] + actual_files
        )

        if returncode != 0:
            return returncode

        # Reformat the resulting edits via: git cl format.
        _RunGit(['cl', 'format'] + actual_files)
        return 0

    finally:
        # No matter what, unstage the git changes.
        _RunGit(['reset', '--quiet', 'HEAD'] + actual_files)


def RunTestsForProject(spanify_dir, scripts_dir, src_dir, project):
    test_dir = os.path.join(spanify_dir, 'tests', project)
    if not os.path.isdir(test_dir):
        return 0, 0

    print('\nTesting spanify in %s\n' % test_dir)

    source_files = glob.glob(os.path.join(test_dir, '*-original.cc'))
    actual_files = [
        '-'.join([f.rsplit('-', 1)[0], 'actual.cc']) for f in source_files
    ]
    expected_files = [
        '-'.join([f.rsplit('-', 1)[0], 'expected.cc']) for f in source_files
    ]

    if not source_files:
        print('No test files found for project %s.' % project)
        return 0, 0

    include_paths = [
        os.path.realpath(src_dir),
        os.path.realpath(os.path.join(src_dir, 'testing/gtest/include')),
        os.path.realpath(os.path.join(src_dir, 'testing/gmock/include')),
        os.path.realpath(
            os.path.join(src_dir,
                         'third_party/googletest/src/googletest/include')),
        os.path.realpath(
            os.path.join(src_dir,
                         'third_party/googletest/src/googlemock/include')),
    ]

    # Set up the test environment.
    for source, actual in zip(source_files, actual_files):
        shutil.copyfile(source, actual)

    # Generate a temporary compilation database to run the tool over.
    compile_database = os.path.join(test_dir, 'compile_commands.json')
    with open(compile_database, 'w') as f:
        f.write(_GenerateCompileCommands(actual_files, include_paths))

    # Run the tool.
    old_cwd = os.getcwd()
    os.chdir(test_dir)
    try:
        exitcode = _ApplyTool(scripts_dir, spanify_dir, test_dir, actual_files)
    finally:
        os.chdir(old_cwd)

    if exitcode != 0:
        return 0, len(source_files)

    # Compare actual-vs-expected results.
    passed = 0
    failed = 0
    for expected, actual in zip(expected_files, actual_files):
        print('[ RUN      ] %s' % os.path.relpath(actual, spanify_dir))
        with open(expected, 'r') as f:
            expected_output = f.readlines()
        with open(actual, 'r') as f:
            actual_output = f.readlines()

        if actual_output != expected_output:
            failed += 1
            lines = difflib.unified_diff(
                expected_output,
                actual_output,
                fromfile=os.path.relpath(expected, spanify_dir),
                tofile=os.path.relpath(actual, spanify_dir))
            sys.stdout.writelines(lines)
            print('[  FAILED  ] %s' % os.path.relpath(actual, spanify_dir))
            # Don't clean up the file on failure, so the results can be referenced
            # more easily.
        else:
            print('[       OK ] %s' % os.path.relpath(actual, spanify_dir))
            passed += 1
            os.remove(actual)

    if failed == 0:
        os.remove(compile_database)

    return passed, failed


def main():
    spanify_dir = os.path.dirname(os.path.realpath(__file__))
    clang_dir = os.path.dirname(spanify_dir)
    scripts_dir = os.path.join(clang_dir, 'scripts')
    src_dir = os.path.dirname(os.path.dirname(clang_dir))

    total_passed = 0
    total_failed = 0

    for project in ['chrome', 'partition_alloc']:
        passed, failed = RunTestsForProject(spanify_dir, scripts_dir, src_dir,
                                            project)
        total_passed += passed
        total_failed += failed

    print('\n[==========] Total tests ran: %d' % (total_passed + total_failed))
    if (total_passed + total_failed) == 0:
        print('No tests were found.')
        return 1
    if total_passed > 0:
        print('[  PASSED  ] %d tests.' % total_passed)
    if total_failed > 0:
        print('[  FAILED  ] %d tests.' % total_failed)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

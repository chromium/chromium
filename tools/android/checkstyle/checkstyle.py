#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that is used by PRESUBMIT.py to run style checks on Java files."""

import argparse
import collections
import concurrent.futures
import math
import os
import platform
import subprocess
import sys
import threading
import xml.dom.minidom


_SELF_DIR = os.path.dirname(__file__)
CHROMIUM_SRC = os.path.normpath(os.path.join(_SELF_DIR, '..', '..', '..'))
_CHECKSTYLE_CIPD_DIR = os.path.join(
    CHROMIUM_SRC, 'third_party', 'checkstyle', 'cipd'
)
_CHECKSTYLE_ROOT = os.path.join(_CHECKSTYLE_CIPD_DIR, 'checkstyle-all.jar')
# Native build of checkstyle. Only available for Linux x64.
_CHECKSTYLE_BINARY = os.path.join(_CHECKSTYLE_CIPD_DIR, 'checkstyle')
_JAVA_PATH = os.path.join(
    CHROMIUM_SRC, 'third_party', 'jdk', 'current', 'bin', 'java'
)
_STYLE_FILE = os.path.join(_SELF_DIR, 'chromium-style-5.0.xml')
_REMOVE_UNUSED_IMPORTS_PATH = os.path.join(
    _SELF_DIR, 'remove_unused_imports.py'
)
_INCLUSIVE_WARNING_IDENTIFIER = 'Please use inclusive language'
_INCLUSIVE_HEADER = (
    '  ^^^ The above edited file(s) contain non-inclusive language '
    '(may be pre-existing). ^^^  '
)
_LARGE_BATCH_THRESHOLD = 100


class Violation(
    collections.namedtuple('Violation', 'file,line,column,message,severity')
):
    def __str__(self):
        column = f'{self.column}:' if self.column else ''
        return f'{self.file}:{self.line}:{column} {self.message}'

    def is_warning(self):
        return self.severity == 'warning'

    def is_error(self):
        return self.severity == 'error'

    def sort_key(self):
        return (
            self.file,
            self.line if isinstance(self.line, int) else 0,
            self.column or 0,
        )


class _CheckstyleError(Exception):
    pass


def _use_native_binary():
    return sys.platform.startswith('linux') and platform.machine() == 'x86_64'


def _checkstyle_command(style_file, java_files):
    if _use_native_binary():
        cmd = [_CHECKSTYLE_BINARY]
    else:
        cmd = [
            _JAVA_PATH,
            '-Xmx512m',
        ]
        # TieredStopAtLevel=1 restricts HotSpot to the C1 compiler, reducing JVM
        # startup latency for typical short-lived presubmit runs. For large batch
        # runs (>= _LARGE_BATCH_THRESHOLD files), allow C2 JIT compilation for peak
        # throughput.
        if len(java_files) < _LARGE_BATCH_THRESHOLD:
            cmd.append('-XX:TieredStopAtLevel=1')
        cmd.extend(
            [
                '-cp',
                _CHECKSTYLE_ROOT,
                'com.puppycrawl.tools.checkstyle.Main',
            ]
        )
    return cmd + ['-c', style_file, '-f', 'xml'] + java_files


def _parse_violations(local_path, returncode, stdout, stderr):
    stderr_lines = stderr.splitlines()
    # One line is always: "Checkstyle ends with # warnings/errors".
    if len(stderr_lines) > 1 or (
        stderr_lines and 'ends with' not in stderr_lines[0]
    ):
        raise _CheckstyleError(
            f'{stderr}\nCheckstyle failed with returncode={returncode}.\n'
            'This might mean you have a syntax error'
        )

    try:
        root = xml.dom.minidom.parseString(stdout)
    except Exception as e:
        raise _CheckstyleError(
            f'Tried to parse:\n{stdout}\n'
            f'Checkstyle failed with returncode={returncode}.\n{e}'
        ) from e

    inclusive_files = []
    inclusive_warning = ''
    results = []
    for fileElement in root.getElementsByTagName('file'):
        filename = fileElement.attributes['name'].value
        if filename.startswith(local_path):
            filename = filename[len(local_path) + 1 :]
        errors = fileElement.getElementsByTagName('error')
        for error in errors:
            severity = error.attributes['severity'].value
            if severity not in ('warning', 'error'):
                continue
            message = error.attributes['message'].value
            line = int(error.attributes['line'].value)
            column = None
            if error.hasAttribute('column'):
                column = int(error.attributes['column'].value)
            if _INCLUSIVE_WARNING_IDENTIFIER in message:
                inclusive_warning = message
                inclusive_files.append(f'{filename}:{str(line)}\n  ')
                continue
            results.append(Violation(filename, line, column, message, severity))

    if inclusive_files:
        results.append(
            Violation(
                ''.join(str(filename) for filename in inclusive_files) + '\n',
                _INCLUSIVE_HEADER,
                '',
                inclusive_warning,
                'warning',
            )
        )

    return results


def _shard_files(java_files, cpu_count=None):
    # Checkstyle is single-threaded. Shard large runs across available CPUs,
    # keeping at least 100 files per shard (so normal CLs spawn only 1 process)
    # and at most 400 files per shard (to bound command-line length).
    cpu_count = cpu_count or os.cpu_count() or 1
    shard_size = max(100, min(400, math.ceil(len(java_files) / cpu_count)))
    return [
        java_files[i : i + shard_size]
        for i in range(0, len(java_files), shard_size)
    ]


def _run_checkstyle_shard(local_path, style_file, java_files):
    cmd = _checkstyle_command(style_file, java_files)
    result = subprocess.run(
        cmd, capture_output=True, check=False, text=True, cwd=CHROMIUM_SRC
    )
    return _parse_violations(
        local_path, result.returncode, result.stdout, result.stderr
    )


def run_checkstyle(local_path, style_file, java_files):
    if not java_files:
        return []
    shards = _shard_files(java_files)
    try:
        if len(shards) == 1:
            return _run_checkstyle_shard(local_path, style_file, shards[0])
        violations = []
        with concurrent.futures.ThreadPoolExecutor() as executor:
            futures = [
                executor.submit(
                    _run_checkstyle_shard, local_path, style_file, shard
                )
                for shard in shards
            ]
            for future in futures:
                violations.extend(future.result())
        violations.sort(key=Violation.sort_key)
        return violations
    except _CheckstyleError as e:
        sys.stderr.write(f'{e}\n')
        sys.exit(-1)


def run_presubmit(input_api, output_api, files_to_skip=None):
    # Android toolchain is only available on Linux.
    if not sys.platform.startswith('linux'):
        return []

    # Filter out non-Java files and files that were deleted.
    java_files = [
        x.AbsoluteLocalPath()
        for x in input_api.AffectedSourceFiles(
            lambda f: input_api.FilterSourceFile(f, files_to_skip=files_to_skip)
        )
        if x.LocalPath().endswith('.java')
    ]
    if not java_files:
        return []

    local_path = input_api.PresubmitLocalPath()
    shards = _shard_files(java_files, input_api.cpu_count)

    lock = threading.Lock()
    all_violations = []
    error_messages = []
    num_completed = 0

    def parse_output(returncode, stdout, stderr):
        nonlocal num_completed
        try:
            violations = _parse_violations(
                local_path, returncode, stdout, stderr
            )
            error_msg = None
        except _CheckstyleError as e:
            violations = []
            error_msg = str(e)

        with lock:
            num_completed += 1
            is_last = num_completed == len(shards)
            all_violations.extend(violations)
            if error_msg is not None:
                error_messages.append(error_msg)

            if not is_last:
                return []

            if error_messages:
                return [output_api.PresubmitError('\n'.join(error_messages))]

            all_violations.sort(key=Violation.sort_key)
            warnings = ['  ' + str(v) for v in all_violations if v.is_warning()]
            errors = ['  ' + str(v) for v in all_violations if v.is_error()]

            ret = []
            if warnings:
                ret.append(
                    output_api.PresubmitPromptWarning('\n'.join(warnings))
                )
            if errors:
                msg = '\n'.join(errors)
                if 'Unused import:' in msg or 'Duplicate import' in msg:
                    msg += """

To remove unused imports: """ + input_api.os_path.relpath(
                        _REMOVE_UNUSED_IMPORTS_PATH, local_path
                    )
                ret.append(output_api.PresubmitError(msg))
            return ret

    return input_api.RunTests(
        [
            input_api.Command(
                name='checkstyle',
                cmd=_checkstyle_command(_STYLE_FILE, shard),
                kwargs={},
                output_parser=parse_output,
            )
            for shard in shards
        ]
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--all',
        action='store_true',
        help='Run checkstyle on all non-third_party Java files in the repo.',
    )
    parser.add_argument('java_files', nargs='*')
    args = parser.parse_args()

    if args.all:
        if args.java_files:
            parser.error('Cannot specify both --all and java_files.')
        result = subprocess.run(
            ['git', '-c', 'core.quotePath=false', 'ls-files', '--', '*.java'],
            capture_output=True,
            check=True,
            text=True,
            cwd=CHROMIUM_SRC,
        )
        java_files = [
            p
            for f in result.stdout.splitlines()
            if 'third_party' not in f.split('/')
            and os.path.exists(p := os.path.join(CHROMIUM_SRC, f))
        ]
    elif args.java_files:
        java_files = [os.path.abspath(f) for f in args.java_files]
    else:
        parser.error('Must specify either --all or at least one java_file.')

    violations = run_checkstyle(CHROMIUM_SRC, _STYLE_FILE, java_files)
    if args.all:
        violations = [v for v in violations if v.is_error()]
    for v in violations:
        print(f'{v} ({v.severity})')

    if any(v.is_error() for v in violations):
        sys.exit(1)


if __name__ == '__main__':
    main()

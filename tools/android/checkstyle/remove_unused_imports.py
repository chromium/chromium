#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uses checkstyle to find unused imports and then removes them."""

import os
import pathlib
import subprocess
import sys

import checkstyle

_STYLE_FILE = os.path.join(os.path.dirname(__file__), 'unused-imports.xml')


def _query_for_java_files():
    git_root = subprocess.check_output('git rev-parse --show-toplevel',
                                       shell=True,
                                       encoding='utf8').strip()
    result = subprocess.run(
        ('git diff --name-only -M '
         '$(git merge-base @{u} HEAD 2>/dev/null || echo HEAD^)'),
        capture_output=True,
        check=True,
        shell=True,
        encoding='utf8')
    paths = (os.path.join(git_root, x) for x in result.stdout.splitlines()
             if x.endswith('.java'))
    return [p for p in paths if os.path.exists(p)]


def _delete_line(path, line):
    lines = path.read_text().splitlines(keepends=True)
    lines.pop(line - 1)
    # Check if removed last import from an import group.
    if lines[line - 2:line] == ['\n', '\n']:
        lines.pop(line - 1)

    path.write_text(''.join(lines))


def main():
    java_files = _query_for_java_files()
    if not java_files:
        print('There are no changed .java files.')
        sys.exit(1)
    os.chdir(checkstyle.CHROMIUM_SRC)
    violations = checkstyle.run_checkstyle(checkstyle.CHROMIUM_SRC,
                                           _STYLE_FILE, java_files)
    processed = set()
    for v in sorted(violations, key=lambda x: -x.line):
        # Guard against multiple warnings on the same line.
        key = (v.file, v.line)
        if key not in processed:
            _delete_line(pathlib.Path(v.file), v.line)
            processed.add(key)
    count = len(processed)
    print(f'Removed {count} unused import{"s" if count != 1 else ""}.')


if __name__ == '__main__':
    main()

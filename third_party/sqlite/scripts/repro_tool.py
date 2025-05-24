#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests building sqlite and optionally running a sql script.

Generates amalgamations, builds the sqlite shell, and optionally runs a sql
file in the sqlite shell. Designed to be passed to `git bisect run` to
determine a culprit in the sqlite repro for either an issue in the build or a
sqlite script.

Example usage:
  git bisect start c12b0d5b7135 9e45bccab2b8
  git bisect run ../scripts/repro_tool.py repro.sql --should_build
"""

import os
import argparse
import subprocess
import sys
import textwrap

from functools import cache
from pathlib import PurePath


class SqlTester:

    def __init__(self, build_dir, quiet, verbose, should_build):
        self.relative_build_dir = build_dir
        self.quiet = quiet
        self.verbose = verbose and not quiet
        self.should_build = should_build

    @property
    @cache
    def script_dir(self):
        return os.path.dirname(os.path.realpath(__file__))

    @property
    @cache
    def chrome_root(self):
        result = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                                encoding='UTF-8',
                                cwd=self.script_dir,
                                capture_output=True,
                                text=True)
        return result.stdout.strip()

    @property
    @cache
    def build_dir(self):
        return os.path.join(self.chrome_root, self.relative_build_dir)

    @property
    @cache
    def generate_amalgamation_script(self):
        return os.path.join(self.script_dir, 'generate_amalgamation.py')

    @property
    @cache
    def sqlite_shell(self):
        return os.path.join(self.build_dir, 'sqlite_shell')

    def run_process(self, process, stdin=None):
        stdout = None if self.verbose else open(os.devnull, 'w')
        process = subprocess.Popen(process,
                                   encoding='UTF-8',
                                   cwd=self.chrome_root,
                                   stdin=stdin,
                                   stdout=stdout,
                                   stderr=subprocess.PIPE)
        _, stderr = process.communicate()
        return process.returncode, stderr

    def generate_amalgamation(self):
        return self.run_process(self.generate_amalgamation_script)

    def build_sqlite_shell(self):
        return self.run_process(
            ['autoninja', '-C', self.build_dir, 'sqlite_shell'])

    def run_sql(self, sqlfile):
        sqlfileobject = open(sqlfile, 'r', encoding='UTF-8')
        return self.run_process(self.sqlite_shell, stdin=sqlfileobject)

    def print(self, message):
        if not self.quiet:
            print(message)

    def print_stderr(self, stderr):
        self.print(textwrap.indent(stderr, '    | '))

    def handle_build_error(self, stderr):
        if self.should_build:
            self.print('  Unexpectedly failed:')
            self.print_stderr(stderr)

            # -1 indicates to `git bisect run` to abort:
            # https://git-scm.com/docs/git-bisect#_bisect_run
            return -1

        self.print('  Failed:')
        self.print_stderr(stderr)
        return 1

    def run(self, sqlfile):
        if not os.path.isfile(self.generate_amalgamation_script):
            self.print('generate_amalgamation.py no longer exists in the same '
                       'directory as this script or has been renamed.')

            # -1 indicates to `git bisect run` to abort:
            # https://git-scm.com/docs/git-bisect#_bisect_run
            return -1

        self.print('Generating amalgamations')
        returncode, stderr = self.generate_amalgamation()

        if returncode != 0:
            return self.handle_build_error(stderr)

        self.print('Building sqlite_shell')
        returncode, stderr = self.build_sqlite_shell()

        if returncode != 0:
            return self.handle_build_error(stderr)

        if sqlfile == None:
            return 0

        self.print('Running sqlfile')
        returncode, stderr = self.run_sql(sqlfile)

        if returncode != 0:
            self.print('  Failed:')
            self.print_stderr(stderr)
            return 1

        self.print('  Success!')
        return 0


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument(
        '-C',
        dest='build_dir',
        default='out/Default',
        nargs='?',
        required=False,
        help='The Chromium build directory. This is always relative the root '
        'of the Chromium repo.')

    parser.add_argument(
        '-q',
        '--quiet',
        dest='quiet',
        required=False,
        action='store_true',
        help='Don\'t print to console. Takes precedence over the verbose '
        'flag.')

    parser.add_argument(
        '-v',
        '--verbose',
        dest='verbose',
        required=False,
        action='store_true',
        help='Print subprocess output. The quiet flag takes precedence.')

    parser.add_argument(
        '--should_build',
        dest='should_build',
        required=False,
        action='store_true',
        help=
        'If the build fails, exits with code that indicates `git bisect run` '
        'to abort.')

    parser.add_argument('sqlfile',
                        nargs='?',
                        help='Path to the sqlite file that repros a crash')

    args = parser.parse_args()

    sqlTester = SqlTester(args.build_dir, args.quiet, args.verbose,
                          args.should_build)

    return sqlTester.run(args.sqlfile)


if __name__ == '__main__':
    sys.exit(main())

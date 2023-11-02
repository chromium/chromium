#!/usr/bin/env python3

# Copyright 2019 The Crashpad Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import collections
import os
import subprocess
import sys

MigInterface = collections.namedtuple(
    'MigInterface', ['user_c', 'server_c', 'user_h', 'server_h'])


def generate_interface(defs,
                       interface,
                       includes=[],
                       sdk=None,
                       clang_path=None,
                       mig_path=None,
                       migcom_path=None,
                       arch=None):
    if mig_path is None:
        mig_path = 'mig'

    # yapf: disable
    command = [
        mig_path,
        '-user', interface.user_c,
        '-server', interface.server_c,
        '-header', interface.user_h,
        '-sheader', interface.server_h,
    ]
    # yapf: enable

    if clang_path is not None:
        os.environ['MIGCC'] = clang_path
    if migcom_path is not None:
        os.environ['MIGCOM'] = migcom_path
    if arch is not None:
        command.extend(['-arch', arch])
    if sdk is not None:
        command.extend(['-isysroot', sdk])
    for include in includes:
        command.extend(['-I' + include])
    command.append(defs)
    subprocess.check_call(command)


def parse_args(args, multiple_arch=False):
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang-path', help='Path to clang')
    parser.add_argument('--mig-path', help='Path to mig')
    parser.add_argument('--migcom-path', help='Path to migcom')
    if not multiple_arch:
        parser.add_argument('--arch', help='Target architecture')
    else:
        parser.add_argument(
            '--arch',
            default=[],
            action='append',
            help='Target architecture (may appear multiple times)')
    parser.add_argument('--sdk', help='Path to SDK')
    parser.add_argument(
        '--include',
        default=[],
        action='append',
        help='Additional include directory (may appear multiple times)')
    parser.add_argument('defs')
    parser.add_argument('user_c')
    parser.add_argument('server_c')
    parser.add_argument('user_h')
    parser.add_argument('server_h')
    return parser.parse_args(args)


def main(args):
    parsed = parse_args(args)
    interface = MigInterface(parsed.user_c, parsed.server_c, parsed.user_h,
                             parsed.server_h)
    generate_interface(parsed.defs, interface, parsed.include, parsed.sdk,
                       parsed.clang_path, parsed.mig_path, parsed.migcom_path,
                       parsed.arch)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

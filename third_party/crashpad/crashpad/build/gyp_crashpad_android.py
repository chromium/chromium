#!/usr/bin/env python
# coding: utf-8

# Copyright 2017 The Crashpad Authors. All rights reserved.
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
import glob
import gyp_crashpad
import os
import re
import subprocess
import sys


def main(args):
  parser = argparse.ArgumentParser(
      description='Set up an Android cross build',
      epilog='Additional arguments will be passed to gyp_crashpad.py.')
  parser.add_argument('--arch', required=True, help='Target architecture')
  parser.add_argument('--api-level', required=True, help='Target API level')
  parser.add_argument('--ndk', required=True, help='Standalone NDK toolchain')
  (parsed, extra_command_line_args) = parser.parse_known_args(args)

  ndk_bin_dir = os.path.join(parsed.ndk,
                             'toolchains',
                             'llvm',
                             'prebuilt',
                             'linux-x86_64',
                             'bin')
  if not os.path.exists(ndk_bin_dir):
    parser.error("missing toolchain")

  ARCH_TO_ARCH_TRIPLET = {
    'arm': 'armv7a-linux-androideabi',
    'arm64': 'aarch64-linux-android',
    'ia32': 'i686-linux-android',
    'x64': 'x86_64-linux-android',
  }

  clang_prefix = ARCH_TO_ARCH_TRIPLET[parsed.arch] + parsed.api_level
  os.environ['CC_target'] = os.path.join(ndk_bin_dir, clang_prefix + '-clang')
  os.environ['CXX_target'] = os.path.join(ndk_bin_dir, clang_prefix + '-clang++')

  extra_args = ['-D', 'android_api_level=' + parsed.api_level]

  # ARM only includes 'v7a' in the tool prefix for clang
  tool_prefix = ('arm-linux-androideabi' if parsed.arch == 'arm'
                 else ARCH_TO_ARCH_TRIPLET[parsed.arch])

  for tool in ('ar', 'nm', 'readelf'):
    os.environ['%s_target' % tool.upper()] = (
        os.path.join(ndk_bin_dir, '%s-%s' % (tool_prefix, tool)))

  return gyp_crashpad.main(
      ['-D', 'OS=android',
       '-D', 'target_arch=%s' % parsed.arch,
       '-D', 'clang=1',
       '-f', 'ninja-android'] +
      extra_args +
      extra_command_line_args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os
import platform

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))
_CWD = os.getcwd()

_NODE_PATH = os.path.join(_SRC_PATH, 'third_party', 'node')
sys.path.append(_NODE_PATH)
import node
import node_modules

_ESLINT_CONFIG_TEMPLATE = """import {defaultConfig, %(extraConfigs)s} from '%(config_base)s';

export default [
  ...defaultConfig,
  %(extraConfigs)s
  {
    languageOptions: {
      parserOptions: {
        'project': ['%(tsconfig)s'],
      },
    },
  },
];"""

# A subset of the 'X errors and Y warnings potentially fixable with the `--fix`
# option.' instruction that is emitted by ESLint when automatically fixable
# errors exist. This is used to strip this line from the error output to avoid
# any confusion, when using the `--fix` flag wouldn't actually work because the
# input files are generated files.
_TOKEN_TO_STRIP = 'potentially fixable with the `--fix` option'


def _generate_config_file(args):
  config_file = os.path.join(args.out_folder, 'eslint.config.mjs')
  with open(config_file, 'w', newline='', encoding='utf-8') as f:
    extra_configs = []
    if args.enable_web_component_missing_deps:
      extra_configs.append('webComponentMissingDepsConfig')
    if args.enable_no_chrome_send:
      extra_configs.append('noChromeSendConfig')
    if args.enable_no_explicit_any:
      extra_configs.append('noExplicitAnyConfig')

    f.write(
        _ESLINT_CONFIG_TEMPLATE % {
            'config_base':
                args.config_base,
            'tsconfig':
                args.tsconfig,
            'extraConfigs':
                '' if len(extra_configs) == 0 else ', '.join(extra_configs) +
                ',',
        })
    return config_file


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--config_base', required=True)
  parser.add_argument('--custom_loader_script', required=True)
  parser.add_argument('--tsconfig', required=True)
  parser.add_argument(
      '--enable_web_component_missing_deps', action='store_true')
  parser.add_argument('--enable_no_chrome_send', action='store_true')
  parser.add_argument('--enable_no_explicit_any', action='store_true')
  parser.add_argument('--in_files', nargs='*', required=True)

  args = parser.parse_args(argv)

  config_file = _generate_config_file(args)
  if len(args.in_files) == 0:
    return

  in_files = list(map(lambda f: os.path.join(args.in_folder, f), args.in_files))

  chunk_size = len(in_files)
  if sys.platform == 'win32' and len(' '.join(in_files)) > 30000:
    # On Windows the maximum command line length for Python's CreateProcess()
    # function is 32767 characters. When the limit is exceeded a confusing
    # "FileNotFoundError: [WinError 206] The filename or extension is too long"
    # error is thrown. Split to multiple invocations to work around that.
    chunk_size = min(round(len(in_files) / 2), 100)

  in_files_chunks = [
      in_files[i:i + chunk_size] for i in range(0, len(in_files), chunk_size)
  ]

  for files in in_files_chunks:
    custom_loader_script = os.path.abspath(args.custom_loader_script)
    if platform.system() == "Windows":
      # Need to prepend 'file:///' to prevent errors like the following one:
      # "Error [ERR_UNSUPPORTED_ESM_URL_SCHEME]: Only URLs with a scheme in:
      # file, data, and node are supported by the default ESM loader. On
      # Windows, absolute paths must be valid file:// URLs. Received protocol
      # 'c:'"
      custom_loader_script = 'file:///' + custom_loader_script

    node.RunNode([
        '--loader',
        custom_loader_script,
        node_modules.PathToEsLint(),
        # Force colored output, otherwise no colors appear when running locally.
        '--color',
        '--quiet',
        '--config',
        config_file,
    ] + files)


if __name__ == '__main__':
  main(sys.argv[1:])

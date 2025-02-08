#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))
_CWD = os.getcwd()

_NODE_PATH = os.path.join(_SRC_PATH, 'third_party', 'node')
sys.path.append(_NODE_PATH)
import node
import node_modules

_ESLINT_CONFIG_TEMPLATE = """import path from 'path';

import defaultConfig from '%(config_base)s';

export default [
  defaultConfig,
  {
    languageOptions: {
      parserOptions: {
        'project': [path.join(import.meta.dirname, './%(tsconfig)s')],
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


def _generate_config_file(out_dir, config_base, tsconfig):
  config_file = os.path.join(out_dir, 'eslint.config.mjs')
  with open(config_file, 'w', newline='', encoding='utf-8') as f:
    f.write(_ESLINT_CONFIG_TEMPLATE % {
        'config_base': config_base,
        'tsconfig': tsconfig
    })
    return config_file


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--config_base', required=True)
  parser.add_argument('--tsconfig', required=True)
  parser.add_argument('--in_files', nargs='*', required=True)

  args = parser.parse_args(argv)

  config_file = _generate_config_file(args.out_folder, args.config_base,
                                      args.tsconfig)
  if len(args.in_files) == 0:
    return

  node.RunNode([
      node_modules.PathToEsLint(),
      # Force colored output, otherwise no colors appear when running locally.
      '--color',
      '--quiet',
      '--config',
      config_file,
  ] + list(map(lambda f: os.path.join(args.in_folder, f), args.in_files)))


if __name__ == '__main__':
  main(sys.argv[1:])

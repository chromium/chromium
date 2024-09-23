#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Run `cargo vet` against `third_party/rust/chromium_crates_io`.

Arguments are passed through to `cargo vet`.
'''

import argparse
import os
import platform
import subprocess
import sys

from run_cargo import (RunCargo, DEFAULT_SYSROOT)

_SCRIPT_NAME = os.path.basename(__file__)
_THIS_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROMIUM_ROOT_DIR = os.path.join(_THIS_DIR, '..', '..')
_MANIFEST_DIR = os.path.join(_CHROMIUM_ROOT_DIR, 'third_party', 'rust',
                             'chromium_crates_io')
_VET_DATA_DIR = os.path.join(_MANIFEST_DIR, 'supply-chain')
_CONFIG_TOML_PATH = os.path.join(_VET_DATA_DIR, 'config.toml')
_IMPORTS_LOCK_PATH = os.path.join(_VET_DATA_DIR, 'imports.lock')


def AreOnlyCommentsChanged(old_contents, new_contents):

    def NonCommentLines(contents):
        lines = contents.splitlines()
        lines = [line for line in lines if line and not line.startswith('#')]
        return lines

    return NonCommentLines(old_contents) == NonCommentLines(new_contents)


def main():
    parser = argparse.ArgumentParser(
        description=
        'run `cargo vet` against `//third_party/rust/chromium_crates_io`')
    parser.add_argument('--rust-sysroot',
                        default=DEFAULT_SYSROOT,
                        help='use cargo and rustc from here')
    (args, unrecognized_args) = parser.parse_known_args()

    # Avoid clobbering `config.toml` - see
    # https://github.com/mozilla/cargo-vet/issues/589 and note that `gnrt
    # vendor` generates this file from the `vet_config.toml.hbs` template.
    with open(_CONFIG_TOML_PATH, 'r') as f:
        old_config_toml = f.read()

    _CARGO_ARGS = ['-Zunstable-options', '-C', _MANIFEST_DIR]
    _EXTRA_VET_ARGS = ['--cargo-arg=-Zbindeps', '--no-registry-suggestions']
    success = RunCargo(
        args.rust_sysroot, None,
        _CARGO_ARGS + ['vet'] + unrecognized_args + _EXTRA_VET_ARGS)

    # Unclober `config.toml` changes if desirable.
    with open(_CONFIG_TOML_PATH, 'r') as f:
        new_config_toml = f.read()
    if new_config_toml != old_config_toml:
        if AreOnlyCommentsChanged(old_config_toml, new_config_toml):
            print(f"{_SCRIPT_NAME}: NOTE: Restoring `config.toml` " \
                   "(comment-only changes detected)")
            with open(_CONFIG_TOML_PATH, 'w') as f:
                f.write(old_config_toml)
        else:
            print(f"{_SCRIPT_NAME}: WARNING: Detected non-trivial " \
                   "`config.toml` changes. " \
                   "Check if `vet_config.toml.hbs` needs to be updated.")

    if not success:
        is_presubmit = '--locked' in unrecognized_args and \
                       '--frozen' in unrecognized_args
        if is_presubmit:
            print("INFO: Chromium's `cargo vet` policy and presubmit " \
                  "are described in `//docs/rust-unsafe.md`.")

    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())

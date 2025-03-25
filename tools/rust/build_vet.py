#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the cargo-vet tool.'''

# TODO(https://crbug.com/405980483): Evaluate whether to keep supporting
# `tools/crates/run_cargo_vet.py` (and therefore whether to keep building
# `cargo-vet` binary).  Note that we have removed `cargo vet` presubmits (as
# tracked in https://crbug.com/405980483).

import argparse
import os
import sys

from build_bindgen import (RunCargo)
from update_rust import (RUST_TOOLCHAIN_OUT_DIR)

VET_VERSION = '0.9.1'


def main():
    parser = argparse.ArgumentParser(description='Build and package cargo-vet')
    args, rest = parser.parse_known_args()

    print(f'Building cargo-vet...')
    cargo_args = [
        'install', 'cargo-vet', '--locked', f'--version={VET_VERSION}',
        f'--root={RUST_TOOLCHAIN_OUT_DIR}', '--force', '--no-track'
    ]
    RunCargo(cargo_args)

    return 0


if __name__ == '__main__':
    sys.exit(main())

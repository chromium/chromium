#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# BUG: How do we get link-cplusplus to link libc++ into its crate? It generates
# from its build.rs `cargo:rustc-link-lib=c++` which we could handle, and then
# do nothing here. Or we can add a flag to third_party.toml to have us add
# libc++ (via a dummy c++ source set) as a dependency of link-cplusplus.

from __future__ import annotations

import argparse
import sys

from lib import download
from lib import gen

parser = argparse.ArgumentParser(description="Third-party Rust crate tools.")
subparsers = parser.add_subparsers()

download_parser = subparsers.add_parser("download")
download_parser.add_argument(
    "crate_name", help="The crate name to be downloaded from crates.io")
download_parser.add_argument(
    "crate_version",
    help=("The version of the crate to be downloaded from crates.io. " +
          "Usually just the major version number, such as '1'"))
download_parser.add_argument(
    "--security-critical",
    required=True,
    choices=["yes", "no"],
    help="Whether the crate is considered to be security critical")
download_parser.add_argument(
    "--license",
    type=str,
    help=("The license of the crate, if an approved license can't be found " +
          "automatically"))
download_parser.add_argument("--verbose",
                             action="store_true",
                             help="Used for debugging of this tool.")
download_parser.set_defaults(func=download.run)

gen_parser = subparsers.add_parser("gen")
gen_parser.add_argument(
    "--target",
    help=("The single target to generate BUILD.gn files for, from " +
          "'rustc --print=target-list'. When not specified, the BUILD.gn " +
          "files are generated for all Chromium targets. Used for faster " +
          "debugging of this tool."))
gen_parser.add_argument("--verbose",
                        action="store_true",
                        help="Used for debugging of this tool.")
gen_parser.add_argument(
    "--with-tests",
    action="store_true",
    help="Whether to generate build rules for crate unit tests.")
gen_parser.set_defaults(func=gen.run)

args = parser.parse_args()
if "func" not in args:
    parser.print_usage()
else:
    args.func(args)

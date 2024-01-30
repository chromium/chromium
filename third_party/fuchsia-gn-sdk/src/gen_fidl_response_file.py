#!/usr/bin/env python3.8
# Copyright 2019 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is a copy of
# https://fuchsia.googlesource.com/garnet/+/731fec4559ba459b0d2567a2e68363a5d0021259/public/lib/fidl/build/fidl/gen_response_file.py

import argparse
import os
import string
import sys


def read_libraries(libraries_path):
    with open(libraries_path) as f:
        lines = f.readlines()
        return [l.rstrip("\n") for l in lines]


def write_libraries(libraries_path, libraries):
    directory = os.path.dirname(libraries_path)
    if not os.path.exists(directory):
        os.makedirs(directory)
    with open(libraries_path, "w+") as f:
        for library in libraries:
            f.write(library)
            f.write("\n")


def main(args_list=None):
    parser = argparse.ArgumentParser(
        description="Generate response file for FIDL frontend")
    parser.add_argument(
        "--out-response-file",
        help="The path for for the response file to generate",
        required=True)
    parser.add_argument(
        "--out-libraries",
        help="The path for for the libraries file to generate",
        required=True)
    parser.add_argument(
        "--json", help="The path for the JSON file to generate, if any")
    parser.add_argument(
        "--name", help="The name for the generated FIDL library, if any")
    parser.add_argument(
        "--sources", help="List of FIDL source files", nargs="*")
    parser.add_argument(
        "--dep-libraries", help="List of dependent libraries", nargs="*")
    parser.add_argument(
        "--target-api-level", help="The target Fuchsia API level", type=int)
    parser.add_argument(
        "--experimental",
        help="An experimental flag to enable",
        action="append")
    if args_list:
        args = parser.parse_args(args_list)
    else:
        args = parser.parse_args()

    target_libraries = []

    for dep_libraries_path in args.dep_libraries or []:
        dep_libraries = read_libraries(dep_libraries_path)
        for library in dep_libraries:
            if library in target_libraries:
                continue
            target_libraries.append(library)
    target_libraries.append(" ".join(sorted(args.sources)))
    write_libraries(args.out_libraries, target_libraries)

    response_file = []

    if args.json:
        response_file.append("--json %s" % args.json)

    if args.name:
        response_file.append("--name %s" % args.name)

    if args.target_api_level:
        response_file.append("--available fuchsia:%d" % args.target_api_level)

    if args.experimental:
        response_file.extend(
            "--experimental %s" % flag for flag in args.experimental)

    response_file.extend("--files %s" % library for library in target_libraries)

    with open(args.out_response_file, "w+") as f:
        f.write(" ".join(response_file))
        f.write("\n")


if __name__ == "__main__":
    sys.exit(main())

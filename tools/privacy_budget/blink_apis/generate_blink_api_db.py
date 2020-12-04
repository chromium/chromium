#!/usr/bin/env vpython

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "2.7"
# wheel: <
#   name: "infra/python/wheels/protobuf-py2_py3"
#   version: "version:3.13.0"
# >
#
# wheel: <
#   name: "infra/python/wheels/six-py2_py3"
#   version: "version:1.11.0"
# >
#
# [VPYTHON:END]
"""
Generate a database of Blink APIs.

The schema is described in proto/blink_apis.proto. It has the following goals:

  * Capture enough information to follow the transitive dependencies of an API.
    For example if a certain dictionary contains a known identifiable datum,
    then any attribute or operation returning this dictionary should also be
    considered sensitive.

  * Capture identifiability annotations and other relevant bits of information
    in the form of Extended Attributes.

  * Be a method for keeping track of how APIs are introduced, updated, and
    removed.

  * Be a method for looking up which APIs correspond to a `UseCounter`.

  * Be a method for looking up the APIs that are affected by a runtime feature.

To generate this API DB, run the following in a Chromium build directory:

    ninja blink_apis

This should create a file named `blink_apis.textpb` in the root build
directory. E.g. 'out/Debug'`
"""

import sys
import argparse


def parse_options():
    parser = argparse.ArgumentParser(description="%prog [options]")
    parser.add_argument("--web_idl_database",
                        type=str,
                        help="filepath of the input database")
    parser.add_argument("--web_feature_mojom",
                        type=str,
                        help="path of web_feature.mojom")
    parser.add_argument("--output", type=str, help="filepath of output file")
    parser.add_argument("--path", type=str, help="Additions to sys.path")
    parser.add_argument(
        "--chromium_revision",
        type=str,
        help="Chromium revision (git hash) for the source of Blink WebIDL DB")
    args = parser.parse_args()

    required_option_names = ("web_idl_database", "output", "web_feature_mojom")
    for opt_name in required_option_names:
        if getattr(args, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if args.path:
        for p in args.path.split(':'):
            sys.path.append(p)

    return args


def main():
    args = parse_options()

    from blink_api_proto import BlinkApiProto
    from web_feature import WebFeature
    w = WebFeature(args.web_feature_mojom)
    p = BlinkApiProto(args.web_idl_database, args.output,
                      args.chromium_revision, w)
    p.Parse()
    p.WriteTo(args.output)


if __name__ == '__main__':
    main()

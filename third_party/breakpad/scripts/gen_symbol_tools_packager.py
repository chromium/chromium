#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generates `package_symbol_tools.py` in the out directory, which:
- builds `upload_system_symbols` (written in Go, so unable to be built
  directly through Chromium's BUILD files)
- packages it, along with the remaining tools and scripts necessary to upload
  system symbols on a macOS system.
Packages everything necessary to extract and upload system symbols on a macOS
system into `symbol_tools.zip` in the out directory.
"""
import argparse
import os
import sys


def get_indent(template, token):
    idx = template.find(token)
    if idx == -1:
        raise RuntimeError(f"{token} not found")
    cursor = idx
    while cursor > 0 and template[cursor - 1] == " ":
        cursor -= 1
    return idx - cursor


def replace(template, token, replacement):
    if isinstance(replacement, list):
        joiner = ',\n' + get_indent(template, token) * ' '
        quoted = [f'"{el}"' for el in replacement]
        return template.replace(token, joiner.join(quoted))
    return template.replace(token, replacement)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-t", required=True, help="Template file")
    parser.add_argument("-a", required=True, help="Architecture to build for")
    parser.add_argument("-go_src", nargs="+", required=True,
                        help="Go source dir for `upload_system_symbols")
    parser.add_argument("-extra", nargs="+", required=True,
                        help="Extra files to include in the zip")

    args = parser.parse_args()
    if args.a == "x64":
        args.a = "amd64"
    with open(args.t) as f:
        template = f.read()

    replacements = [
        ("@ARCH@", args.a),
        ("@GO_SOURCE_DIR@", args.go_src),
        ("@EXTRA@", args.extra),
        ("@CWD@", os.getcwd())
    ]
    for token, replacement in replacements:
        template = replace(template, token, replacement)

    with open("package_symbol_tools.py", "w") as f:
        f.write(template)
    return True


if __name__ == '__main__':
    sys.exit(0 if main() else 1)

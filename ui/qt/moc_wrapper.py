#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument(
    'input', type=str, help='Input header file.')
parser.add_argument(
    'output', type=str, help='Output file.')
parser.add_argument(
    '--path', required=False, type=str, default=None,
    help='Path to moc binary.')

args = parser.parse_args()

if args.path is None:
    subprocess.check_call(["moc", args.input, "-o", args.output])
else:
    subprocess.check_call([args.path + "/moc", args.input, "-o", args.output])

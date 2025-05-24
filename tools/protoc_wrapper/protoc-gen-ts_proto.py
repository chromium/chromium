#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
import subprocess

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..'))
_CWD = os.getcwd()
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))

import node
import node_modules
import argparse


def main(argv):
  cmd = [node.GetBinaryPath(), node_modules.PathToTsProto()] + argv
  process = subprocess.run(cmd)


if __name__ == '__main__':
  main(sys.argv[1:])
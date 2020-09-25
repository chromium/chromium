#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script to run wayland-scaner for wayland_protocol.gni.
"""

from __future__ import print_function

import argparse
import os.path
import subprocess
import sys

def generate_code(wayland_scanner_cmd, code_type, path_in, path_out):
  ret = subprocess.call([wayland_scanner_cmd, code_type, path_in, path_out])
  if ret != 0:
    raise RuntimeError("wayland-scanner returned an error: %d" % ret)

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument("--cmd", help="wayland-scanner command to execute")
  parser.add_argument("--src-root", help="Root source directory")
  parser.add_argument("--root-gen-dir", help="Directory for generated files")
  parser.add_argument("protocols", nargs="+",
                      help="Input protocol file paths relative to src root.")

  options = parser.parse_args()
  cmd = os.path.realpath(options.cmd)
  src_root = options.src_root
  root_gen_dir = options.root_gen_dir
  protocols = options.protocols

  for protocol in protocols:
    protocol_path = os.path.join(src_root, protocol)
    protocol_without_extension = protocol.rsplit(".", 1)[0]
    out_base_name = os.path.join(root_gen_dir, protocol_without_extension)
    generate_code(cmd, "private-code", protocol_path,
                  out_base_name + "-protocol.c")
    generate_code(cmd, "client-header", protocol_path,
                  out_base_name + "-client-protocol.h")
    generate_code(cmd, "server-header", protocol_path,
                  out_base_name + "-server-protocol.h")

if __name__ == "__main__":
  try:
    main(sys.argv)
  except RuntimeError as e:
    print(e, file=sys.stderr)
    sys.exit(1)

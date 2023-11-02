#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
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

def generate_code(wayland_scanner_cmd, scanner_args, path_in, path_out):
  ret = subprocess.call([wayland_scanner_cmd, *scanner_args, path_in, path_out])
  if ret != 0:
    raise RuntimeError("wayland-scanner returned an error: %d" % ret)

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument("--cmd", help="wayland-scanner command to execute")
  parser.add_argument("--src-root", help="Root source directory")
  parser.add_argument("--root-gen-dir", help="Directory for generated files")
  parser.add_argument("protocols", nargs="+",
                      help="Input protocol file paths relative to src root.")
  parser.add_argument("--generator-type", help="Controls what type of\
                      headers/code shall be generated", default="all",
                      choices={"all", "protocol-client", "protocol-server",\
                      "protocol-marshalling"})

  options = parser.parse_args()
  cmd = os.path.realpath(options.cmd)
  src_root = options.src_root
  root_gen_dir = options.root_gen_dir
  protocols = options.protocols
  generator_type = options.generator_type

  version = subprocess.check_output([cmd, "--version"],
                                    stderr=subprocess.STDOUT).decode("utf-8")
  # The version is of the form "wayland-scanner 1.18.0\n"
  version = tuple([int(x) for x in version.strip().split(" ")[1].split(".")])
  # This needs to generate private-code to avoid ODR
  # violation and avoid hacks such as in https://crrev.com/c/2416941/
  # See third_party/wayland/BUILD.gn#212 there for details there.
  private_code_type = "private-code" if version > (1, 14, 90) else "code"

  for protocol in protocols:
    protocol_path = os.path.join(src_root, protocol)
    protocol_without_extension = protocol.rsplit(".", 1)[0]
    out_base_name = os.path.join(root_gen_dir, protocol_without_extension)
    if generator_type == "protocol-marshalling":
      generate_code(cmd, [ private_code_type ], protocol_path,
                    out_base_name + "-protocol.c")
    elif generator_type == "protocol-client":
      scanner_args = [ "client-header" ]
      generate_code(cmd, scanner_args, protocol_path,
                    out_base_name + "-client-protocol.h")
      scanner_args.append("-c")
      generate_code(cmd, scanner_args, protocol_path,
                    out_base_name + "-client-protocol-core.h")
    elif generator_type == "protocol-server":
      scanner_args = [ "server-header" ]
      generate_code(cmd, scanner_args, protocol_path,
                    out_base_name + "-server-protocol.h")
      scanner_args.append("-c")
      generate_code(cmd, scanner_args, protocol_path,
                    out_base_name + "-server-protocol-core.h")
    else:
      assert(generator_type == "all")

      generate_code(cmd, [ private_code_type ], protocol_path,
                    out_base_name + "-protocol.c")
      generate_code(cmd, [ "client-header" ], protocol_path,
                    out_base_name + "-client-protocol.h")
      generate_code(cmd, [ "server-header" ], protocol_path,
                    out_base_name + "-server-protocol.h")


if __name__ == "__main__":
  try:
    main(sys.argv)
  except RuntimeError as e:
    print(e, file=sys.stderr)
    sys.exit(1)

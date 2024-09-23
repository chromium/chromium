#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A simple wrapper for protoc.
Script for //third_party/protobuf/proto_library.gni .
Features:
- Inserts #include for extra header automatically.
- Prevents bad proto names.
- Works around protoc's bad descriptor file generation.
  Ninja expects the format:
  target: deps
  But protoc just outputs:
  deps
  This script adds the "target:" part.
"""

from __future__ import print_function
import argparse
import os.path
import subprocess
import sys
import tempfile

PROTOC_INCLUDE_POINT = "// @@protoc_insertion_point(includes)"


def FormatGeneratorOptions(options):
  if not options:
    return ""
  if options.endswith(":"):
    return options
  return options + ":"


def VerifyProtoNames(protos):
  for filename in protos:
    if "-" in filename:
      raise RuntimeError("Proto file names must not contain hyphens "
                         "(see http://crbug.com/386125 for more information).")


def StripProtoExtension(filename):
  if not filename.endswith(".proto"):
    raise RuntimeError("Invalid proto filename extension: "
                       "{0} .".format(filename))
  return filename.rsplit(".", 1)[0]


def WriteIncludes(headers, include):
  for filename in headers:
    include_point_found = False
    contents = []
    with open(filename) as f:
      for line in f:
        stripped_line = line.strip()
        contents.append(stripped_line)
        if stripped_line == PROTOC_INCLUDE_POINT:
          if include_point_found:
            raise RuntimeError("Multiple include points found.")
          include_point_found = True
          extra_statement = "#include \"{0}\"".format(include)
          contents.append(extra_statement)

      if not include_point_found:
        raise RuntimeError("Include point not found in header: "
                           "{0} .".format(filename))

    with open(filename, "w") as f:
      for line in contents:
        print(line, file=f)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument("--protoc", required=True,
                      help="Relative path to compiler.")

  parser.add_argument("--proto-in-dir", required=True,
                      help="Base directory with source protos.")
  parser.add_argument("--cc-out-dir",
                      help="Output directory for standard C++ generator.")
  parser.add_argument("--py-out-dir",
                      help="Output directory for standard Python generator.")
  parser.add_argument("--js-out-dir",
                      help="Output directory for standard JS generator.")
  parser.add_argument("--protoc-gen-js",
                      help="Relative path to javascript compiler.")

  parser.add_argument("--plugin-out-dir",
                      help="Output directory for custom generator plugin.")

  parser.add_argument('--enable-kythe-annotations', action='store_true',
                      help='Enable generation of Kythe kzip, used for '
                      'codesearch.')
  parser.add_argument("--plugin",
                      help="Relative path to custom generator plugin.")
  parser.add_argument("--plugin-options",
                      help="Custom generator plugin options.")
  parser.add_argument("--cc-options",
                      help="Standard C++ generator options.")
  parser.add_argument("--include",
                      help="Name of include to insert into generated headers.")
  parser.add_argument("--import-dir", action="append", default=[],
                      help="Extra import directory for protos, can be repeated."
  )
  parser.add_argument("--descriptor-set-out",
                      help="Path to write a descriptor.")
  parser.add_argument(
      "--descriptor-set-dependency-file",
      help="Path to write the dependency file for descriptor set.")
  # The meaning of this flag is flipped compared to the corresponding protoc
  # flag due to this script previously passing --include_imports. Removing the
  # --include_imports is likely to have unintended consequences.
  parser.add_argument(
      "--exclude-imports",
      help="Do not include imported files into generated descriptor.",
      action="store_true",
      default=False)
  parser.add_argument("protos", nargs="+",
                      help="Input protobuf definition file(s).")

  options = parser.parse_args(argv)

  proto_dir = os.path.relpath(options.proto_in_dir)
  protoc_cmd = [os.path.realpath(options.protoc)]

  protos = options.protos
  headers = []
  VerifyProtoNames(protos)

  if options.py_out_dir:
    protoc_cmd += ["--python_out", options.py_out_dir]

  if options.js_out_dir:
    protoc_cmd += [
        "--js_out",
        "one_output_file_per_input_file,binary:" + options.js_out_dir,
        "--plugin=protoc-gen-js=" + os.path.realpath(options.protoc_gen_js),
    ]

  if options.cc_out_dir:
    cc_out_dir = options.cc_out_dir
    cc_options_list = []
    if options.enable_kythe_annotations:
      cc_options_list.extend([
          'annotate_headers', 'annotation_pragma_name=kythe_metadata',
          'annotation_guard_name=KYTHE_IS_RUNNING'
      ])

    # cc_options will likely have trailing colon so needs to be inserted at the
    # end.
    if options.cc_options:
      cc_options_list.append(options.cc_options)

    cc_options = FormatGeneratorOptions(','.join(cc_options_list))
    protoc_cmd += ["--cpp_out", cc_options + cc_out_dir]
    for filename in protos:
      stripped_name = StripProtoExtension(filename)
      headers.append(os.path.join(cc_out_dir, stripped_name + ".pb.h"))

  if options.plugin_out_dir:
    plugin_options = FormatGeneratorOptions(options.plugin_options)
    protoc_cmd += [
      "--plugin", "protoc-gen-plugin=" + os.path.relpath(options.plugin),
      "--plugin_out", plugin_options + options.plugin_out_dir
    ]

  protoc_cmd += ["--proto_path", proto_dir]
  for path in options.import_dir:
    # TODO: crbug.com/1477926 - Do not specify unused `--import-dir`s.
    # On a remote worker, it shows `warning: directory does not exist` when
    # there are no dependencies under the directory.
    if os.path.exists(path):
      protoc_cmd += ["--proto_path", path]

  protoc_cmd += [os.path.join(proto_dir, name) for name in protos]

  if options.descriptor_set_out:
    protoc_cmd += ["--descriptor_set_out", options.descriptor_set_out]
    if not options.exclude_imports:
      protoc_cmd += ["--include_imports"]

  dependency_file_data = None
  if options.descriptor_set_out and options.descriptor_set_dependency_file:
    protoc_cmd += ['--dependency_out', options.descriptor_set_dependency_file]
    ret = subprocess.call(protoc_cmd)

    with open(options.descriptor_set_dependency_file, 'rb') as f:
      dependency_file_data = f.read().decode('utf-8')

  ret = subprocess.call(protoc_cmd)
  if ret != 0:
    if ret <= -100:
      # Windows error codes such as 0xC0000005 and 0xC0000409 are much easier to
      # recognize and differentiate in hex. In order to print them as unsigned
      # hex we need to add 4 Gig to them.
      error_number = "0x%08X" % (ret + (1 << 32))
    else:
      error_number = "%d" % ret
    raise RuntimeError("Protoc has returned non-zero status: "
                       "{0}".format(error_number))

  if dependency_file_data:
    with open(options.descriptor_set_dependency_file, 'w') as f:
      f.write(dependency_file_data)

  if options.include:
    WriteIncludes(headers, options.include)


if __name__ == "__main__":
  try:
    main(sys.argv[1:])
  except RuntimeError as e:
    print(e, file=sys.stderr)
    sys.exit(1)

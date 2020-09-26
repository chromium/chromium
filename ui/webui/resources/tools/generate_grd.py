# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates a grit grd file from a list of input manifest files. This is useful
# for preventing the need to list JS files in multiple locations, as files can
# be listed just once in the BUILD.gn file as inputs for a build rule that knows
# how to output such a manifest (e.g. preprocess_grit).
#
# Variables:
#   manifest-files:
#      List of paths to manifest files. Each must contain a JSON object with
#      2 fields:
#      - base_dir, the base directory where the files are located
#      - files, a list of file paths from the base directory
#
#   out_grd:
#     Path where the generated grd file should be written.
#
#   grd_prefix:
#     Used to generate both grit IDs for included files and names for the
#     header/pak/resource map files specified in the <outputs> section of the
#     grd file. For example, prefix "foo" will result in a grd file that has
#     as output "foo_resources.pak", "grit/foo_resources.h", etc, and has grit
#     IDs prefixed by IDS_FOO.
#
#   root_gen_dir:
#     Path to the root generated directory. Used to compute the relative path
#     from the root generated directory for setting file paths, as grd files
#     with generated file paths must specify these paths as
#     "${root_gen_dir}/<path_to_file>"

import argparse
import json
import os
import sys

_CWD = os.getcwd()

GRD_BEGIN_TEMPLATE = '<?xml version="1.0" encoding="UTF-8"?>\n'\
                     '<grit latest_public_release="0" current_release="1" '\
                     'output_all_resource_defines="false">\n'\
                     '  <outputs>\n'\
                     '    <output filename="grit/{prefix}_resources.h" '\
                     'type="rc_header">\n'\
                     '      <emit emit_type=\'prepend\'></emit>\n'\
                     '    </output>\n'\
                     '    <output filename="grit/{prefix}_resources_map.cc"\n'\
                     '            type="resource_file_map_source" />\n'\
                     '    <output filename="grit/{prefix}_resources_map.h"\n'\
                     '            type="resource_map_header" />\n'\
                     '    <output filename="{prefix}_resources.pak" '\
                     'type="data_package" />\n'\
                     '  </outputs>\n'\
                     '  <release seq="1">\n'\
                     '    <includes>\n'

GRD_INCLUDE_TEMPLATE = '      <include name="{name}" ' \
                       'file="${{root_gen_dir}}/{path_from_gen}" ' \
                       'use_base_dir="false" type="BINDATA" />\n'

GRD_END_TEMPLATE = '    </includes>\n'\
                   '  </release>\n'\
                   '</grit>\n'

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--manifest-files', required=True, nargs="*")
  parser.add_argument('--out-grd', required=True)
  parser.add_argument('--grd-prefix', required=True)
  parser.add_argument('--root-gen-dir', required=True)
  args = parser.parse_args(argv)

  grd_file = open(os.path.normpath(os.path.join(_CWD, args.out_grd)), 'w')
  grd_file.write(GRD_BEGIN_TEMPLATE.format(prefix=args.grd_prefix))

  for manifest_file in args.manifest_files:
    manifest_path = os.path.normpath(os.path.join(_CWD, manifest_file))
    with open(manifest_path, 'r') as f:
      data = json.load(f)
      base_dir= os.path.normpath(os.path.join(_CWD, data['base_dir']))
      for filename in data['files']:
        name_suffix = filename.upper().replace('/', '_').replace('.', '_')
        name = 'IDR_%s_%s' % (args.grd_prefix.upper(), name_suffix)
        filepath = os.path.join(base_dir, filename).replace('\\', '/')
        rebased_path = os.path.relpath(filepath, args.root_gen_dir)
        grd_file.write(GRD_INCLUDE_TEMPLATE.format(name=name,
                                                   path_from_gen=rebased_path))

  grd_file.write(GRD_END_TEMPLATE)
  return


if __name__ == '__main__':
  main(sys.argv[1:])

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates a grit grd file from a list of input manifest files. This is useful
# for preventing the need to list JS files in multiple locations, as files can
# be listed just once in the BUILD.gn file as inputs for a build rule that knows
# how to output such a manifest (e.g. preprocess_if_expr).
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
#
#   input_files:
#     List of file paths (from |input_files_base_dir|) that are not included in
#     any manifest files, but should be added to the grd.
#
#   input_files_base_dir:
#     The base directory for the paths in |input_files|. |input_files| and
#     |input_files_base_dir| must either both be provided or both be omitted.

import argparse
import json
import os
import sys

_CWD = os.getcwd()

GRD_BEGIN_TEMPLATE = '<?xml version="1.0" encoding="UTF-8"?>\n'\
                     '<grit latest_public_release="0" current_release="1" '\
                     'output_all_resource_defines="false">\n'\
                     '  <outputs>\n'\
                     '    <output filename="{out_dir}/{prefix}_resources.h" '\
                     'type="rc_header">\n'\
                     '      <emit emit_type=\'prepend\'></emit>\n'\
                     '    </output>\n'\
                     '    <output filename="{out_dir}/{prefix}_resources_map.cc"\n'\
                     '            type="resource_file_map_source" />\n'\
                     '    <output filename="{out_dir}/{prefix}_resources_map.h"\n'\
                     '            type="resource_map_header" />\n'\
                     '    <output filename="{prefix}_resources.pak" '\
                     'type="data_package" />\n'\
                     '  </outputs>\n'\
                     '  <release seq="1">\n'\
                     '    <includes>\n'

GRD_INCLUDE_TEMPLATE = '      <include name="{name}" ' \
                       'file="{file}" resource_path="{path}" ' \
                       'use_base_dir="false" type="{type}" />\n'

GRD_END_TEMPLATE = '    </includes>\n'\
                   '  </release>\n'\
                   '</grit>\n'

GRDP_BEGIN_TEMPLATE = '<?xml version="1.0" encoding="UTF-8"?>\n'\
                     '<grit-part>\n'
GRDP_END_TEMPLATE = '</grit-part>\n'

# Generates an <include .... /> row for the given file.
def _generate_include_row(grd_prefix, filename, pathname, \
                          resource_path_rewrites, resource_path_prefix):
  assert '\\' not in filename
  assert '\\' not in pathname
  name_suffix = filename.upper().replace('/', '_').replace('.', '_'). \
          replace('-', '_').replace('@', '_AT_')
  name = 'IDR_%s_%s' % (grd_prefix.upper(), name_suffix)
  extension = os.path.splitext(filename)[1]
  type = 'chrome_html' if extension == '.html' or extension == '.js' \
          else 'BINDATA'

  resource_path = resource_path_rewrites[filename] \
      if filename in resource_path_rewrites else filename

  if resource_path_prefix != None:
    resource_path = resource_path_prefix + '/' + resource_path
  assert '\\' not in resource_path

  return GRD_INCLUDE_TEMPLATE.format(
      file=pathname,
      path=resource_path,
      name=name,
      type=type)


def _generate_part_row(filename):
  return '      <part file="%s" />\n' % filename


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--manifest-files', nargs="*")
  parser.add_argument('--out-grd', required=True)
  parser.add_argument('--grd-prefix', required=True)
  parser.add_argument('--root-gen-dir', required=True)
  parser.add_argument('--input-files', nargs="*")
  parser.add_argument('--input-files-base-dir')
  parser.add_argument('--output-files-base-dir', default='grit')
  parser.add_argument('--grdp-files', nargs="*")
  parser.add_argument('--resource-path-rewrites', nargs="*")
  parser.add_argument('--resource-path-prefix')
  args = parser.parse_args(argv)

  grd_path = os.path.normpath(os.path.join(_CWD, args.out_grd))
  with open(grd_path, 'w', newline='', encoding='utf-8') as grd_file:
    begin_template = GRDP_BEGIN_TEMPLATE if args.out_grd.endswith('.grdp') \
        else GRD_BEGIN_TEMPLATE
    grd_file.write(begin_template.format(prefix=args.grd_prefix,
        out_dir=args.output_files_base_dir))

    if args.grdp_files != None:
      out_grd_dir = os.path.dirname(args.out_grd)
      for grdp_file in args.grdp_files:
        grdp_path = os.path.relpath(grdp_file, out_grd_dir).replace('\\', '/')
        grd_file.write(_generate_part_row(grdp_path))

    resource_path_rewrites = {}
    if args.resource_path_rewrites != None:
      for r in args.resource_path_rewrites:
        [original, rewrite] = r.split("|")
        resource_path_rewrites[original] = rewrite

    if args.input_files != None:
      assert(args.input_files_base_dir)
      args.input_files_base_dir = args.input_files_base_dir.replace('\\', '/')
      args.root_gen_dir = args.root_gen_dir.replace('\\', '/')

      # Detect whether the input files reside under $root_src_dir or
      # $root_gen_dir.
      base_dir = os.path.join('${root_src_dir}', args.input_files_base_dir)
      if args.input_files_base_dir.startswith(args.root_gen_dir + '/'):
        base_dir = args.input_files_base_dir.replace(
            args.root_gen_dir + '/', '${root_gen_dir}/')

      for filename in args.input_files:
        norm_base = os.path.normpath(args.input_files_base_dir)
        norm_path = os.path.normpath(os.path.join(args.input_files_base_dir,
                                                  filename))
        assert os.path.commonprefix([norm_base, norm_path]) == norm_base, \
            f'Error: input_file {filename} found outside of ' + \
            'input_files_base_dir'

        filepath = os.path.join(base_dir, filename).replace('\\', '/')
        grd_file.write(_generate_include_row(
            args.grd_prefix, filename, filepath,
            resource_path_rewrites, args.resource_path_prefix))

    if args.manifest_files != None:
      for manifest_file in args.manifest_files:
        manifest_path = os.path.normpath(os.path.join(_CWD, manifest_file))
        with open(manifest_path, 'r', encoding='utf-8') as f:
          data = json.load(f)
          base_dir= os.path.normpath(os.path.join(_CWD, data['base_dir']))
          for filename in data['files']:
            filepath = os.path.join(base_dir, filename)
            rebased_path = os.path.relpath(filepath, args.root_gen_dir)
            rebased_path = rebased_path.replace('\\', '/')
            grd_file.write(_generate_include_row(
                args.grd_prefix, filename, '${root_gen_dir}/' + rebased_path,
                resource_path_rewrites, args.resource_path_prefix))

    end_template = GRDP_END_TEMPLATE if args.out_grd.endswith('.grdp') else \
        GRD_END_TEMPLATE
    grd_file.write(end_template)
    return


if __name__ == '__main__':
  main(sys.argv[1:])

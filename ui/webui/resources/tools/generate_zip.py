#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to package WebUI resources from a .grd file into a .zip."""

import argparse
import os
import sys
import xml.etree.ElementTree as ET
import zipfile


def _parse_grd_includes(grd_path):
  tree = ET.parse(grd_path)
  root = tree.getroot()
  grd_dir = os.path.dirname(grd_path)
  includes = []

  for elem in root.iter():
    if elem.tag == 'part':
      part_file = elem.attrib.get('file')
      if part_file:
        part_path = os.path.join(grd_dir, part_file)
        includes.extend(_parse_grd_includes(part_path))
    elif elem.tag == 'include':
      file_attr = elem.attrib.get('file')
      resource_path = elem.attrib.get('resource_path')
      if file_attr and resource_path:
        includes.append((file_attr, resource_path))

  return includes


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
    '--in-grd', required=True, help='Path to the input .grd file'
  )
  parser.add_argument(
    '--out-zip', required=True, help='Destination .zip archive path'
  )
  parser.add_argument(
    '--root-gen-dir',
    required=True,
    help='Path to root_gen_dir relative to current dir',
  )
  parser.add_argument(
    '--root-src-dir',
    required=True,
    help='Path to root_src_dir relative to current dir',
  )
  parser.add_argument(
    '--grd-resource-path-prefix',
    default='',
    help='Resource path prefix to strip from destination paths in the zip',
  )
  args = parser.parse_args(argv)

  prefix = args.grd_resource_path_prefix.replace('\\', '/').strip('/')
  if prefix:
    prefix += '/'

  entries = []

  for src_template, resource_path in _parse_grd_includes(args.in_grd):
    src_path = os.path.normpath(
      src_template.replace('${root_gen_dir}', args.root_gen_dir).replace(
        '${root_src_dir}', args.root_src_dir
      )
    )

    dst_rel_path = resource_path.replace('\\', '/')
    if prefix and dst_rel_path.startswith(prefix):
      dst_rel_path = dst_rel_path[len(prefix) :]

    entries.append((dst_rel_path, src_path))

  entries.sort(key=lambda entry: entry[0])

  with zipfile.ZipFile(args.out_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
    for dst_rel_path, src_path in entries:
      zinfo = zipfile.ZipInfo(
        filename=dst_rel_path, date_time=(1980, 1, 1, 0, 0, 0)
      )
      zinfo.compress_type = zipfile.ZIP_DEFLATED
      zinfo.external_attr = 0o644 << 16
      with open(src_path, 'rb') as f:
        zf.writestr(zinfo, f.read())

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

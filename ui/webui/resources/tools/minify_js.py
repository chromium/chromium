#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys
import os

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))
_CWD = os.getcwd()

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules
import re


# Conslidate the file to only one Chromium Copyright header when applicable.
def conslidateChomiumCopyright(input_file, output_file):
  chromium_regex = r'// Copyright (\d{4}) The Chromium Authors\n' + \
                 r'// Use of this source code is governed by a BSD-style ' + \
                 r'license that can be\n// found in the LICENSE file.\n?'
  thirdparty_regex = r'\bCopyright\b(?!.*The Chromium Authors)'
  with open(input_file, 'r', encoding="utf8") as file:
    file_content = file.read()
    # Do not remove all Copyright headers if there is a non Chromium Copyright
    # header.
    if not re.search(thirdparty_regex, file_content):
      matches = list(re.finditer(chromium_regex, file_content))
      if len(matches) > 0:
        file_content = re.sub(chromium_regex, '', file_content)
        years = [int(match.group(1)) for match in matches]
        min_year = min(years)
        copyright_string = f"// Copyright {min_year} The Chromium Authors\n" \
                          "// Use of this source code is governed by a " \
                          "BSD-style license that can be\n" \
                          "// found in the LICENSE file.\n"
        file_content = copyright_string + file_content
    with open(output_file, 'w', encoding="utf8", newline='\n') as out_file:
      out_file.write(file_content)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', nargs='*', required=True)
  parser.add_argument('--out_manifest', required=True)

  args = parser.parse_args(argv)
  out_path = os.path.normpath(
      os.path.join(_CWD, args.out_folder).replace('\\', '/'))
  in_path = os.path.normpath(
      os.path.join(_CWD, args.in_folder).replace('\\', '/'))

  for input_file in args.in_files:
    input_filepath = os.path.join(in_path, input_file)
    output_filepath = os.path.join(out_path, input_file)
    conslidateChomiumCopyright(input_filepath, output_filepath)
    node.RunNode([
        node_modules.PathToTerser(), output_filepath, '--comments',
        '/Copyright|license|LICENSE/', '--output', output_filepath, '--module'
    ])

  manifest_data = {}
  manifest_data['base_dir'] = args.out_folder
  manifest_data['files'] = args.in_files
  with open(os.path.normpath(os.path.join(_CWD, args.out_manifest)), 'w',
            newline='', encoding='utf-8') \
      as manifest_file:
    json.dump(manifest_data, manifest_file)


if __name__ == '__main__':
  main(sys.argv[1:])

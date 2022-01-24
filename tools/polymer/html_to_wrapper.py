# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Genaretes a file that exports an HTML Polymer template from a JS/TS file. This
# is useful for implementing Web Components using JS modules, where all the HTML
# needs to reside in the JS file (no more HTML imports).

import argparse
import sys
import io
from os import path, getcwd, makedirs

_CWD = getcwd()

_EXPORT_TEMPLATE = 'import {html} from \
\'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';\n\
export function getTemplate() {\n\
  return html`<!--_html_template_start_-->%s\
<!--_html_template_end_-->`;\n\
}'


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))
  extension = '.ts'

  results = []

  for in_file in args.in_files:
    with io.open(path.join(in_folder, in_file), encoding='utf-8',
                 mode='r') as f:
      html_template = f.read()
      js_export = _EXPORT_TEMPLATE % html_template

      out_folder_for_file = path.join(out_folder, path.dirname(in_file))
      makedirs(out_folder_for_file, exist_ok=True)
      with io.open(path.join(out_folder, in_file) + extension, mode='wb') as f:
        f.write(js_export.encode('utf-8'))
  return


if __name__ == '__main__':
  main(sys.argv[1:])

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Genaretes a wrapper TS file around a source HTML file holding either
#  1) a Polymer element template or
#  2) an <iron-iconset-svg> definitions
#
# Note: The HTML file must be named either 'icons.html' or be suffixed with
# '_icons.html' for this tool to treat them as #2. Consequently, files holding
# Polymer element templates should not use such naming to be treated as #1.
#
# In case #1 the wrapper exports a getTemplate() function that can be used at
# runtime to import the template. This is useful for implementing Web Components
# using JS modules, where all the HTML needs to reside in a JS file (no more
# HTML imports).
#
# In case #2 the wrapper adds the <iron-iconset-svg> element to <head>, so that
# it can be used by <iron-icon> instances.

import argparse
import sys
import io
from os import path, getcwd, makedirs

_CWD = getcwd()

# Template for non-Polymer elements.
_NON_POLYMER_ELEMENT_TEMPLATE = """import {getTrustedHTML} from \'chrome://resources/js/static_types.js\';
export function getTemplate() {
  return getTrustedHTML`<!--_html_template_start_-->%s<!--_html_template_end_-->`;
}"""

# Template for Polymer elements.
_ELEMENT_TEMPLATE = """import {html} from \'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js\';
export function getTemplate() {
  return html`<!--_html_template_start_-->%s<!--_html_template_end_-->`;
}"""

_ICONS_TEMPLATE = """import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const template = html`%s`;
document.head.appendChild(template.content);
"""


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  parser.add_argument('--template',
                      choices=['polymer', 'native'],
                      default='polymer')

  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))
  extension = '.ts'

  results = []

  for in_file in args.in_files:
    with io.open(path.join(in_folder, in_file), encoding='utf-8',
                 mode='r') as f:
      html_content = f.read()

      wrapper = None
      template = _ELEMENT_TEMPLATE \
          if args.template == 'polymer' else _NON_POLYMER_ELEMENT_TEMPLATE

      filename = path.basename(in_file)
      if filename == 'icons.html' or filename.endswith('_icons.html'):
        template = _ICONS_TEMPLATE

      wrapper = template % html_content

      out_folder_for_file = path.join(out_folder, path.dirname(in_file))
      makedirs(out_folder_for_file, exist_ok=True)
      with io.open(path.join(out_folder, in_file) + extension, mode='wb') as f:
        f.write(wrapper.encode('utf-8'))
  return


if __name__ == '__main__':
  main(sys.argv[1:])

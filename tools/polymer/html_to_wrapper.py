# Copyright 2022 The Chromium Authors
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
import io
import shutil
import sys
import tempfile
from os import path, getcwd, makedirs

_HERE_PATH = path.dirname(__file__)
_SRC_PATH = path.normpath(path.join(_HERE_PATH, '..', '..'))
_CWD = getcwd()

sys.path.append(path.join(_SRC_PATH, 'third_party', 'node'))
import node

# Template for non-Polymer elements.
_NON_POLYMER_ELEMENT_TEMPLATE = """import {getTrustedHTML} from '%(scheme)s//resources/js/static_types.js';
export function getTemplate() {
  return getTrustedHTML`<!--_html_template_start_-->%(content)s<!--_html_template_end_-->`;
}"""

# Template for Polymer elements.
_ELEMENT_TEMPLATE = """import {html} from '%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
export function getTemplate() {
  return html`<!--_html_template_start_-->%(content)s<!--_html_template_end_-->`;
}"""

_ICONS_TEMPLATE = """import '%(scheme)s//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {html} from '%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const template = html`%(content)s`;
document.head.appendChild(template.content);
"""


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  parser.add_argument('--minify', action='store_true')
  parser.add_argument('--use_js', action='store_true')
  parser.add_argument('--template',
                      choices=['polymer', 'native'],
                      default='polymer')
  parser.add_argument('--scheme',
                      choices=['chrome', 'relative'],
                      default='chrome')

  args = parser.parse_args(argv)

  in_folder = path.normpath(path.join(_CWD, args.in_folder))
  out_folder = path.normpath(path.join(_CWD, args.out_folder))
  extension = '.js' if args.use_js else '.ts'

  results = []

  # The folder to be used to read the HTML files to be wrapped.
  wrapper_in_folder = in_folder

  if args.minify:
    # Minify the HTML files with html-minifier before generating the wrapper
    # .ts files.
    # Note: Passing all HTML files to html-minifier all at once because
    # passing them individually takes a lot longer.
    # Storing the output in a temporary folder, which is used further below when
    # creating the final wrapper files.
    tmp_out_dir = tempfile.mkdtemp(dir=out_folder)
    try:
      wrapper_in_folder = tmp_out_dir

      # Using the programmatic Node API to invoke html-minifier, because the
      # built-in command line API does not support explicitly specifying
      # multiple files to be processed, and only supports specifying an input
      # folder, which would lead to potentially processing unnecessary HTML
      # files that are not part of the build (stale), or handled by other
      # html_to_wrapper targets.
      node.RunNode(
          [path.join(_HERE_PATH, 'html_minifier.js'), in_folder, tmp_out_dir] +
          args.in_files)
    except RuntimeError as err:
      shutil.rmtree(tmp_out_dir)
      raise err

  # Wrap the input files (minified or not) with an enclosing .ts file.
  for in_file in args.in_files:
    wrapper_in_file = path.join(wrapper_in_folder, in_file)

    with io.open(wrapper_in_file, encoding='utf-8', mode='r') as f:
      html_content = f.read()

      wrapper = None
      template = _ELEMENT_TEMPLATE \
          if args.template == 'polymer' else _NON_POLYMER_ELEMENT_TEMPLATE

      filename = path.basename(in_file)
      if filename == 'icons.html' or filename.endswith('_icons.html'):
        template = _ICONS_TEMPLATE

      wrapper = template % {
          'content': html_content,
          'scheme': 'chrome:' if args.scheme == 'chrome' else '',
      }

      out_folder_for_file = path.join(out_folder, path.dirname(in_file))
      makedirs(out_folder_for_file, exist_ok=True)
      with io.open(path.join(out_folder, in_file) + extension, mode='wb') as f:
        f.write(wrapper.encode('utf-8'))

  if args.minify:
    # Delete the temporary folder that was holding minified HTML files, no
    # longer needed.
    shutil.rmtree(tmp_out_dir)

  return


if __name__ == '__main__':
  main(sys.argv[1:])

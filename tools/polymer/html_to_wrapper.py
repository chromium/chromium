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
import re
import shutil
import sys
import tempfile
from os import path, getcwd, makedirs

_HERE_PATH = path.dirname(__file__)
_SRC_PATH = path.normpath(path.join(_HERE_PATH, '..', '..'))
_CWD = getcwd()

sys.path.append(path.join(_SRC_PATH, 'third_party', 'node'))
import node

# Template for native web component HTML templates.
_NATIVE_ELEMENT_TEMPLATE = """import {getTrustedHTML} from '%(scheme)s//resources/js/static_types.js';
export function getTemplate() {
  return getTrustedHTML`<!--_html_template_start_-->%(content)s<!--_html_template_end_-->`;
}"""

# Template for Polymer web component HTML templates.
_POLYMER_ELEMENT_TEMPLATE = """import {html} from '%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
export function getTemplate() {
  return html`<!--_html_template_start_-->%(content)s<!--_html_template_end_-->`;
}"""

# Template for Lit component HTML templates.
_LIT_ELEMENT_TEMPLATE = """import {html} from '%(scheme)s//resources/lit/v3_0/lit.rollup.js';
import type {%(class_name)s} from './%(file_name)s.js';
%(imports)s
export function getHtml(this: %(class_name)s) {
  return html`<!--_html_template_start_-->%(content)s<!--_html_template_end_-->`;
}"""

# Template for Polymer icon HTML files.
_POLYMER_ICONS_TEMPLATE = """import '%(scheme)s//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import {html} from '%(scheme)s//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const template = html`%(content)s`;
document.head.appendChild(template.content);
"""

# Template for Lit icon HTML files.
_LIT_ICONS_TEMPLATE = """import '%(scheme)s//resources/cr_elements/cr_icon/cr_iconset.js';
import {getTrustedHTML} from '%(scheme)s//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`%(content)s`;
const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
"""

# Tokens used to detect whether the underlying custom element is based on
# Polymer or Lit.
POLYMER_TOKEN = '//resources/polymer/v3_0/polymer/polymer_bundled.min.js'
LIT_TOKEN = '//resources/lit/v3_0/lit.rollup.js'

# Map holding all the different types of HTML files to generate wrappers for.
TEMPLATE_MAP = {
    'lit': _LIT_ELEMENT_TEMPLATE,
    'lit_icons': _LIT_ICONS_TEMPLATE,
    'native': _NATIVE_ELEMENT_TEMPLATE,
    'polymer_icons': _POLYMER_ICONS_TEMPLATE,
    'polymer': _POLYMER_ELEMENT_TEMPLATE,
}


def detect_template_type(definition_file):
  with io.open(definition_file, encoding='utf-8', mode='r') as f:
    content = f.read()

    if POLYMER_TOKEN in content:
      return 'polymer'
    elif LIT_TOKEN in content:
      return 'lit'

    return 'native'


def detect_icon_template_type(icons_file):
  with io.open(icons_file, encoding='utf-8', mode='r') as f:
    content = f.read()
    if 'iron-iconset-svg' in content:
      return 'polymer_icons'
    assert 'cr-iconset' in content, \
        'icons files must include iron-iconset-svg or cr-iconset'
    return 'lit_icons'


_IMPORTS_START_REGEX = '^<!-- #html_wrapper_imports_start$'
_IMPORTS_END_REGEX = '^#html_wrapper_imports_end -->$'


# Extract additional imports to carry over to the HTML wrapper file.
def _extract_import_metadata(file, minify):
  start_line = -1
  end_line = -1

  with io.open(file, encoding='utf-8', mode='r') as f:
    lines = f.read().splitlines()

    for i, line in enumerate(lines):
      if start_line == -1:
        if re.search(_IMPORTS_START_REGEX, line):
          assert end_line == -1
          start_line = i
      else:
        assert end_line == -1

        if re.search(_IMPORTS_END_REGEX, line):
          assert start_line > -1
          end_line = i
          break

  if start_line == -1 or end_line == -1:
    assert start_line == -1
    assert end_line == -1
    return None

  return {
      # Strip metadata from content, unless minification is used, which will
      # strip any HTML comments anyway.
      'content': None if minify else '\n'.join(lines[end_line + 1:]),
      'imports': '\n'.join(lines[start_line + 1:end_line]) + '\n',
  }


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--in_files', required=True, nargs="*")
  parser.add_argument('--minify', action='store_true')
  parser.add_argument('--use_js', action='store_true')
  parser.add_argument('--template',
                      choices=['polymer', 'lit', 'native', 'detect'],
                      default='polymer')
  parser.add_argument('--scheme',
                      choices=['chrome', 'relative'],
                      default='relative')

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

  out_files = []

  # Wrap the input files (minified or not) with an enclosing .ts file.
  for in_file in args.in_files:
    wrapper_in_file = path.join(wrapper_in_folder, in_file)
    template = None
    template_type = args.template
    filename = path.basename(in_file)
    effective_in_file = wrapper_in_file

    if filename == 'icons.html' or filename.endswith('_icons.html'):
      if args.template == 'polymer':
        template_type = 'polymer_icons'
      elif args.template == 'lit':
        template_type = 'lit_icons'
      else:
        assert args.template == 'detect', (
            r'Polymer/Lit icons files not supported with template="%s"' %
            args.template)
        template_type = detect_icon_template_type(wrapper_in_file)
    elif filename.endswith('icons_lit.html'):
      assert args.template == 'lit' or args.template == 'detect', (
          r'Lit icons files not supported with template="%s"' % args.template)
      # Grab the content from the equivalent Polymer file, and substitute
      # cr-iconset for iron-iconset-svg.
      polymer_file = path.join(wrapper_in_folder,
                               in_file.replace('icons_lit', 'icons'))
      effective_in_file = polymer_file
      template_type = 'lit_icons'
    elif template_type == 'detect':
      # Locate the file that holds the web component's definition. Assumed to
      # be in the same folder as input HTML template file.
      definition_file = path.splitext(path.join(in_folder,
                                                in_file))[0] + extension
      template_type = detect_template_type(definition_file)

    with io.open(effective_in_file, encoding='utf-8', mode='r') as f:
      html_content = f.read()

      substitutions = {
          'content': html_content,
          'scheme': 'chrome:' if args.scheme == 'chrome' else '',
      }

      if template_type == 'lit_icons':
        # Replace iron-iconset-svg for the case of Lit icons files generated
        # from a Polymer icons file.
        if 'iron-iconset-svg' in html_content:
          html_content = html_content.replace('iron-iconset-svg', 'cr-iconset')
        substitutions['content'] = html_content
      elif template_type == 'lit':
        # Add Lit specific substitutions.
        basename = path.splitext(path.basename(in_file))[0]
        # Derive class name from file name. For example
        # foo_bar.html -> FooBarElement.
        class_name = ''.join(map(str.title, basename.split('_'))) + 'Element'
        substitutions['class_name'] = class_name
        substitutions['file_name'] = basename

        # Extracting import metadata from original non-minified template.
        import_metadata = _extract_import_metadata(
            path.join(args.in_folder, in_file), args.minify)
        substitutions['imports'] = \
            '' if import_metadata is None else import_metadata['imports']
        if import_metadata is not None and not args.minify:
          # Remove metadata lines from content.
          substitutions['content'] = import_metadata['content']

      wrapper = TEMPLATE_MAP[template_type] % substitutions

      out_folder_for_file = path.join(out_folder, path.dirname(in_file))
      makedirs(out_folder_for_file, exist_ok=True)
      out_file = path.join(out_folder, in_file) + extension
      out_files.append(out_file)
      with io.open(out_file, mode='wb') as f:
        f.write(wrapper.encode('utf-8'))

  if args.minify:
    # Delete the temporary folder that was holding minified HTML files, no
    # longer needed.
    shutil.rmtree(tmp_out_dir)

  return


if __name__ == '__main__':
  main(sys.argv[1:])

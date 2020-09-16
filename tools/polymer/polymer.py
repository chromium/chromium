# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates Polymer3 UI elements (using JS modules) from existing Polymer2
# elements (using HTML imports). This is useful for avoiding code duplication
# while Polymer2 to Polymer3 migration is in progress.
#
# Variables:
#   html_file:
#     The input Polymer2 HTML file to be processed.
#
#   js_file:
#     The input Polymer2 JS file to be processed, or the name of the output JS
#     file when no input JS file exists (see |html_type| below).
#
#   in_folder:
#     The folder where |html_file| and |js_file| (when it exists) reside.
#
#   out_folder:
#     The output folder for the generated Polymer JS file.
#
#   html_type:
#     Specifies the type of the |html_file| such that the script knows how to
#     process the |html_file|. Available values are:
#       dom-module: A file holding a <dom-module> for a UI element (this is
#                   the majority case). Note: having multiple <dom-module>s
#                   within a single HTML file is not currently supported
#       style-module: A file holding a shared style <dom-module>
#                     (no corresponding Polymer2 JS file exists)
#       custom-style: A file holding a <custom-style> (usually a *_vars_css.html
#                     file, no corresponding Polymer2 JS file exists)
#       iron-iconset: A file holding one or more <iron-iconset-svg> instances
#                     (no corresponding Polymer2 JS file exists)
#       v3-ready: A file holding HTML that is already written for Polymer3. A
#                 Polymer3 JS file already exists for such cases. In this mode
#                 HTML content is simply pasted within the JS file. This mode
#                 will be the only supported mode after migration finishes.
#
#   namespace_rewrites:
#     A list of string replacements for replacing global namespaced references
#     with explicitly imported dependencies in the generated JS module.
#     For example "cr.foo.Bar|Bar" will replace all occurrences of "cr.foo.Bar"
#     with "Bar".
#
#   auto_imports:
#     A list of of auto-imports, to inform the script on which variables to
#     import from a JS module. For example "ui/webui/foo/bar/baz.html|Foo,Bar"
#     will result in something like "import {Foo, Bar} from ...;" when
#     encountering any dependency to that file.

import argparse
import io
import os
import re
import sys
from collections import OrderedDict

_CWD = os.getcwd()
_HERE_PATH = os.path.dirname(__file__)
_ROOT = os.path.normpath(os.path.join(_HERE_PATH, '..', '..'))

POLYMER_V1_DIR = 'third_party/polymer/v1_0/components-chromium/'
POLYMER_V3_DIR = 'third_party/polymer/v3_0/components-chromium/'

# Rewrite rules for replacing global namespace references like "cr.ui.Foo", to
# "Foo" within a generated JS module. Populated from command line arguments.
_namespace_rewrites = {}

# Auto-imports map, populated from command line arguments. Specifies which
# variables to import from a given dependency. For example this is used to
# import |FocusOutlineManager| whenever a dependency to
# ui/webui/resources/html/cr/ui/focus_outline_manager.html is encountered.
_auto_imports = {}

# Populated from command line arguments. Specifies a list of HTML imports to
# ignore when converting HTML imports to JS modules.
_ignore_imports = []

_migrated_imports = []

# Populated from command line arguments. Specifies whether "chrome://" URLs
# should be preserved, or whether they should be converted to scheme-relative
# URLs "//" (default behavior).
_preserve_url_scheme = False

# Use an OrderedDict, since the order these redirects are applied matters.
_chrome_redirects = OrderedDict([
    ('chrome://resources/polymer/v1_0/', POLYMER_V1_DIR),
    ('chrome://resources/', 'ui/webui/resources/'),
    ('//resources/polymer/v1_0/', POLYMER_V1_DIR),
    ('//resources/', 'ui/webui/resources/'),
])

_chrome_reverse_redirects = {
    POLYMER_V3_DIR: '//resources/polymer/v3_0/',
    'ui/webui/resources/': '//resources/',
}


# Helper class for converting dependencies expressed in HTML imports, to JS
# imports. |to_js_import()| is the only public method exposed by this class.
# Internally an HTML import path is
#
# 1) normalized, meaning converted from a chrome or scheme-relative or relative
#    URL to to an absolute path starting at the repo's root
# 2) converted to an equivalent JS normalized path
# 3) de-normalized, meaning converted back to a chrome or scheme-relative or
#    relative URL
# 4) converted to a JS import statement
class Dependency:
  def __init__(self, src, dst):
    self.html_file = src
    self.html_path = dst

    if self.html_path.startswith('chrome://'):
      self.input_format = 'chrome'
    elif self.html_path.startswith('//'):
      self.input_format = 'scheme-relative'
    else:
      self.input_format = 'relative'
    self.output_format = self.input_format

    self.html_path_normalized = self._to_html_normalized()
    self.js_path_normalized = self._to_js_normalized()
    self.js_path = self._to_js()

  def _to_html_normalized(self):
    if self.input_format == 'chrome' or self.input_format == 'scheme-relative':
      self.html_path_normalized = self.html_path
      for r in _chrome_redirects:
        if self.html_path.startswith(r):
          self.html_path_normalized = (
              self.html_path.replace(r, _chrome_redirects[r]))
          break
      return self.html_path_normalized

    input_dir = os.path.relpath(os.path.dirname(self.html_file), _ROOT)
    return os.path.normpath(
        os.path.join(input_dir, self.html_path)).replace("\\", "/")

  def _to_js_normalized(self):
    if re.match(POLYMER_V1_DIR, self.html_path_normalized):
      return (self.html_path_normalized
          .replace(POLYMER_V1_DIR, POLYMER_V3_DIR)
          .replace(r'.html', '.js'))

    if self.html_path_normalized == 'ui/webui/resources/html/polymer.html':
      if self.output_format == 'relative':
        self.output_format = 'chrome'
      return POLYMER_V3_DIR + 'polymer/polymer_bundled.min.js'

    if re.match(r'ui/webui/resources/html/', self.html_path_normalized):
      return (self.html_path_normalized
          .replace(r'ui/webui/resources/html/', 'ui/webui/resources/js/')
          .replace(r'.html', '.m.js'))

    extension = (
        '.js' if self.html_path_normalized in _migrated_imports else '.m.js')
    return self.html_path_normalized.replace(r'.html', extension)

  def _to_js(self):
    js_path = self.js_path_normalized

    if self.output_format == 'chrome' or self.output_format == 'scheme-relative':
      for r in _chrome_reverse_redirects:
        if self.js_path_normalized.startswith(r):
          js_path = self.js_path_normalized.replace(
              r, _chrome_reverse_redirects[r])
          break

      # Restore the chrome:// scheme if |preserve_url_scheme| is enabled.
      if _preserve_url_scheme and self.output_format == 'chrome':
        js_path = "chrome:" + js_path
      return js_path

    assert self.output_format == 'relative'
    input_dir = os.path.relpath(os.path.dirname(self.html_file), _ROOT)
    relpath = os.path.relpath(
        self.js_path_normalized, input_dir).replace("\\", "/")
    # Prepend "./" if |relpath| refers to a relative subpath, that is not "../".
    # This prefix is required for JS Modules paths.
    if not relpath.startswith('.'):
      relpath = './' + relpath

    return relpath

  def to_js_import(self, auto_imports):
    if self.html_path_normalized in auto_imports:
      imports = auto_imports[self.html_path_normalized]
      return 'import {%s} from \'%s\';' % (', '.join(imports), self.js_path)

    return 'import \'%s\';' % self.js_path


def _generate_js_imports(html_file):
  output = []
  imports_start_offset = -1
  imports_end_index = -1
  imports_found = False
  with io.open(html_file, encoding='utf-8', mode='r') as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
      match = re.search(r'\s*<link rel="import" href="(.*)"', line)
      if match:
        if not imports_found:
          imports_found = True
          imports_start_offset = i
          # Include the previous line if it is an opening <if> tag.
          if (i > 0):
            previous_line = lines[i - 1]
            if re.search(r'^\s*<if', previous_line):
              imports_start_offset -= 1
              previous_line = '// ' + previous_line
              output.append(previous_line.rstrip('\n'))

        imports_end_index = i - imports_start_offset

        # Convert HTML import URL to equivalent JS import URL.
        dep = Dependency(html_file, match.group(1))
        js_import = dep.to_js_import(_auto_imports)
        if dep.html_path_normalized in _ignore_imports:
          output.append('// ' + js_import)
        else:
          output.append(js_import)

      elif imports_found:
        if re.search(r'^\s*</?if', line):
          line = '// ' + line
        output.append(line.rstrip('\n'))

  if len(output) == 0:
    return output

  # Include the next line if it is a closing </if> tag.
  if re.search(r'^// \s*</if>', output[imports_end_index + 1]):
    imports_end_index += 1

  return output[0:imports_end_index + 1]


def _extract_dom_module_id(html_file):
  with io.open(html_file, encoding='utf-8', mode='r') as f:
    contents = f.read()
    match = re.search(r'\s*<dom-module id="(.*)"', contents)
    assert match
    return match.group(1)


def _add_template_markers(html_template):
  return '<!--_html_template_start_-->%s<!--_html_template_end_-->' % \
      html_template;


def _extract_template(html_file, html_type):
  if html_type == 'v3-ready':
    with io.open(html_file, encoding='utf-8', mode='r') as f:
      template = f.read()
      return _add_template_markers('\n' + template)

  if html_type == 'dom-module':
    with io.open(html_file, encoding='utf-8', mode='r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 2;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 2])
          assert re.match(r'\s*<script ', lines[i - 1])
          end_line = i - 3;
        # Should not have an iron-iconset-svg in a dom-module file.
        assert not re.match(r'\s*<iron-iconset-svg ', line)

    # If an opening <dom-module> tag was found, check that a closing one was
    # found as well.
    if start_line != - 1:
      assert end_line != -1

    return _add_template_markers('\n' + ''.join(lines[start_line:end_line + 1]))

  if html_type == 'style-module':
    with io.open(html_file, encoding='utf-8', mode='r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 1;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 1])
          end_line = i - 1;
    return '\n' + ''.join(lines[start_line:end_line + 1])


  if html_type == 'iron-iconset':
    templates = []
    with io.open(html_file, encoding='utf-8', mode='r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<iron-iconset-svg ', line):
          assert start_line == -1
          assert end_line == -1
          start_line = i;
        if re.match(r'\s*</iron-iconset-svg>', line):
          assert start_line != -1
          assert end_line == -1
          end_line = i
          templates.append(''.join(lines[start_line:end_line + 1]))
          # Reset indices.
          start_line = -1
          end_line = -1
    return '\n' + ''.join(templates)


  assert html_type == 'custom-style'
  with io.open(html_file, encoding='utf-8', mode='r') as f:
    lines = f.readlines()
    start_line = -1
    end_line = -1
    for i, line in enumerate(lines):
      if re.match(r'\s*<custom-style>', line):
        assert start_line == -1
        assert end_line == -1
        start_line = i;
      if re.match(r'\s*</custom-style>', line):
        assert start_line != -1
        assert end_line == -1
        end_line = i;

  return '\n' + ''.join(lines[start_line:end_line + 1])


# Replace various global references with their non-namespaced version, for
# example "cr.ui.Foo" becomes "Foo".
def _rewrite_namespaces(string):
  for rewrite in _namespace_rewrites:
    string = string.replace(rewrite, _namespace_rewrites[rewrite])
  return string


def process_v3_ready(js_file, html_file):
  # Extract HTML template and place in JS file.
  html_template = _extract_template(html_file, 'v3-ready')

  with io.open(js_file, encoding='utf-8') as f:
    lines = f.readlines()

  HTML_TEMPLATE_REGEX = '{__html_template__}'
  for i, line in enumerate(lines):
    line = line.replace(HTML_TEMPLATE_REGEX, html_template)
    lines[i] = line

  out_filename = os.path.basename(js_file)
  return lines, out_filename

def _process_dom_module(js_file, html_file):
  html_template = _extract_template(html_file, 'dom-module')
  js_imports = _generate_js_imports(html_file)

  # Remove IFFE opening/closing lines.
  IIFE_OPENING = '(function() {\n'
  IIFE_OPENING_ARROW = '(() => {\n'
  IIFE_CLOSING = '})();'

  # Remove this line.
  CR_DEFINE_START_REGEX = r'cr.define\('
  # Ignore all lines after this comment, including the line it appears on.
  CR_DEFINE_END_REGEX = r'\s*// #cr_define_end'

  # Replace export annotations with 'export'.
  EXPORT_LINE_REGEX = '/* #export */'

  # Ignore lines with an ignore annotation.
  IGNORE_LINE_REGEX = '\s*/\* #ignore \*/(\S|\s)*'

  with io.open(js_file, encoding='utf-8') as f:
    lines = f.readlines()

  imports_added = False
  iife_found = False
  cr_define_found = False
  cr_define_end_line = -1

  for i, line in enumerate(lines):
    if not imports_added:
      if line.startswith(IIFE_OPENING) or line.startswith(IIFE_OPENING_ARROW):
        assert not cr_define_found, 'cr.define() and IFFE in the same file'
        # Replace the IIFE opening line with the JS imports.
        line = '\n'.join(js_imports) + '\n\n'
        imports_added = True
        iife_found = True
      elif re.match(CR_DEFINE_START_REGEX, line):
        assert not cr_define_found, 'Multiple cr.define()s are not supported'
        assert not iife_found, 'cr.define() and IFFE in the same file'
        line = '\n'.join(js_imports) + '\n\n'
        cr_define_found = True
        imports_added = True
      elif line.startswith('Polymer({\n'):
        # Place the JS imports right before the opening "Polymer({" line.
        line = line.replace(
            r'Polymer({', '%s\n\nPolymer({' % '\n'.join(js_imports))
        imports_added = True

    # Place the HTML content right after the opening "Polymer({" line.
    # Note: There is currently an assumption that only one Polymer() declaration
    # exists per file.
    line = line.replace(
        r'Polymer({',
        'Polymer({\n  _template: html`%s`,' % html_template)

    line = line.replace(EXPORT_LINE_REGEX, 'export')

    if re.match(CR_DEFINE_END_REGEX, line):
      assert cr_define_found, 'Found cr_define_end without cr.define()'
      cr_define_end_line = i
      break

    if re.match(IGNORE_LINE_REGEX, line):
      line = ''

    line = _rewrite_namespaces(line)
    lines[i] = line

  if cr_define_found:
    assert cr_define_end_line != -1, 'No cr_define_end found'
    lines = lines[0:cr_define_end_line]

  if iife_found:
    last_line = lines[-1]
    assert last_line.startswith(IIFE_CLOSING), 'Could not detect IIFE closing'
    lines[-1] = ''

  # Use .m.js extension for the generated JS file, since both files need to be
  # served by a chrome:// URL side-by-side.
  out_filename = os.path.basename(js_file).replace('.js', '.m.js')
  return lines, out_filename

def _process_style_module(js_file, html_file):
  html_template = _extract_template(html_file, 'style-module')
  js_imports = _generate_js_imports(html_file)

  style_id = _extract_dom_module_id(html_file)

  # Add |assetpath| attribute so that relative CSS url()s are resolved
  # correctly. Without this they are resolved with respect to the main HTML
  # documents location (unlike Polymer2). Note: This is assuming that only style
  # modules under ui/webui/resources/ are processed by polymer_modulizer(), for
  # example cr_icons_css.html.
  js_template = \
"""%(js_imports)s
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="%(style_id)s" assetpath="chrome://resources/">%(html_template)s</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));""" % {
      'html_template': html_template,
      'js_imports': '\n'.join(js_imports),
      'style_id': style_id,
  }

  out_filename = os.path.basename(js_file)
  return js_template, out_filename


def _process_custom_style(js_file, html_file):
  html_template = _extract_template(html_file, 'custom-style')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `%(html_template)s`;
document.head.appendChild($_documentContainer.content);""" % {
      'js_imports': '\n'.join(js_imports),
      'html_template': html_template,
  }

  out_filename = os.path.basename(js_file)
  return js_template, out_filename

def _process_iron_iconset(js_file, html_file):
  html_template = _extract_template(html_file, 'iron-iconset')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const template = html`%(html_template)s`;
document.head.appendChild(template.content);
""" % {
      'js_imports': '\n'.join(js_imports),
      'html_template': html_template,
  }

  out_filename = os.path.basename(js_file)
  return js_template, out_filename

def _resetGlobals():
  global _namespace_rewrites
  _namespace_rewrites = {}
  global _auto_imports
  _auto_imports = {}
  global _ignore_imports
  _ignore_imports = []
  global _migrated_imports
  _migrated_imports = []

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_file', required=True)
  parser.add_argument('--html_file', required=True)
  parser.add_argument('--namespace_rewrites', required=False, nargs="*")
  parser.add_argument('--ignore_imports', required=False, nargs="*")
  parser.add_argument('--auto_imports', required=False, nargs="*")
  parser.add_argument('--migrated_imports', required=False, nargs="*")
  parser.add_argument('--preserve_url_scheme', action="store_true")
  parser.add_argument(
      '--html_type', choices=['dom-module', 'style-module', 'custom-style',
      'iron-iconset', 'v3-ready'],
      required=True)
  args = parser.parse_args(argv)

  # Extract namespace rewrites from arguments.
  if args.namespace_rewrites:
    for r in args.namespace_rewrites:
      before, after = r.split('|')
      _namespace_rewrites[before] = after

  # Extract automatic imports from arguments.
  if args.auto_imports:
    global _auto_imports
    for entry in args.auto_imports:
      path, imports = entry.split('|')
      _auto_imports[path] = imports.split(',')

  # Extract ignored imports from arguments.
  if args.ignore_imports:
    assert args.html_type != 'v3-ready'
    global _ignore_imports
    _ignore_imports = args.ignore_imports

  # Extract migrated imports from arguments.
  if args.migrated_imports:
    assert args.html_type != 'v3-ready'
    global _migrated_imports
    _migrated_imports = args.migrated_imports

  # Extract |preserve_url_scheme| from arguments.
  global _preserve_url_scheme
  _preserve_url_scheme = args.preserve_url_scheme

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  js_file = os.path.join(in_folder, args.js_file)
  html_file = os.path.join(in_folder, args.html_file)

  result = ()
  if args.html_type == 'dom-module':
    result = _process_dom_module(js_file, html_file)
  if args.html_type == 'style-module':
    result = _process_style_module(js_file, html_file)
  elif args.html_type == 'custom-style':
    result = _process_custom_style(js_file, html_file)
  elif args.html_type == 'iron-iconset':
    result = _process_iron_iconset(js_file, html_file)
  elif args.html_type == 'v3-ready':
    result = process_v3_ready(js_file, html_file)

  # Reconstruct file.
  # Specify the newline character so that the exact same file is generated
  # across platforms.
  with io.open(os.path.join(out_folder, result[1]), mode='wb') as f:
    for l in result[0]:
      f.write(l.encode('utf-8'))

  # Reset global variables so that main() can be invoked multiple times during
  # testing without leaking state from one test to the next.
  _resetGlobals()
  return


if __name__ == '__main__':
  main(sys.argv[1:])

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates HTML file with all dependencies from a js_unittest build target."""

import os
import sys
from argparse import ArgumentParser

_HTML_FILE = r"""<!DOCTYPE html>
<html>
<script>
// Basic include checker.
window.addEventListener('error', function(e) {
  if ((e.target instanceof HTMLScriptElement)) {
    console.log('ERROR loading <script> element (does it exist?):\n\t' +
                e.srcElement.src + '\n\tIncluded from: ' +
                e.srcElement.baseURI);
  }
}, true /* useCapture */);
</script>
<body>
"""

_CLASSIC_SCRIPT = r'<script src="%s"></script>'
_JS_MODULE = r'<script type="module" src="%s"></script>'
_JS_MODULE_REGISTER_TESTS = r'''
<script>
// Push all entities to global namespace to be visible to the test harness:
// ui/webui/resources/js/webui_resource_test.js
import('%s').then(TestModule => {
  for (const name in TestModule) {
    window[name] = TestModule[name];
  }
});
</script>
'''
_IMPORT = r'<link rel="import" href="%s">'
_HTML_IMPORT_POLYFIL =  _CLASSIC_SCRIPT  % (
    'chrome://resources/polymer/v1_0/html-imports/html-imports.min.js')

_HTML_FOOTER = r"""
</body>
</html>
"""

_ELEMENTS_BUNDLE_IMPORTED = False
_ELEMENTS_BUNDLE = _IMPORT % (
  'chrome://file_manager_test/ui/file_manager/file_manager/foreground/elements'
  '/elements_bundle.html')



def _process_deps(unique_deps, dep_type, target_name):
  """Processes all deps strings, yielding each HTML tag to include the dep.

  Args:
    unique_deps: Iterator of strings, for all deps to be processed.
    dep_type: String: 'classic_script' | 'js_module' |
      'js_module_register_tests' | 'html_import'.
    target_name: Current test target name, used to infer the main Polymer
      element for HTMLImport. element_unitest => element.js/element.html.

  Returns:
    Iterator of strings, each string is a HTML tag <script> or <link>.
  """
  for dep in unique_deps:
    # Scripts from cr_elements are included via HTML imports.
    if '/cr_elements/' in dep:
      continue

    # Special case for jstemplate which has multiple files but we server all of
    # them combined from chrome://resources/js/jstemplate_compiled.js
    if '/jstemplate/' in dep:
      if '/jstemplate.js' in dep:
        yield ('<script src='
               '"chrome://resources/js/jstemplate_compiled.js"></script>')
      # just ignore other files files from /jstemplate/
      continue

    # Ignoring Polymer files, because they're loaded by the HTML imports from
    # other files.
    if 'third_party/polymer/' in dep:
      continue

    # These files are loaded via HTML import. Don't load them again here: that
    # would cause the tests to fail.
    if 'parse_html_subset.js' in dep:
      continue
    if 'i18n_behavior.js' in dep:
      continue

    # Any JS from /elements/ will be HTML imported via the elements_bundle.html.
    if 'file_manager/foreground/elements/' in dep and not'_unittest' in dep:
      global _ELEMENTS_BUNDLE_IMPORTED
      if not _ELEMENTS_BUNDLE_IMPORTED:
        yield _ELEMENTS_BUNDLE
        _ELEMENTS_BUNDLE_IMPORTED = True
      continue

    # Map file_manager files:
    dep = dep.replace('ui/file_manager/',
                      'chrome://file_manager_test/ui/file_manager/', 1)

    # Extern files from closure in //third_party/
    closure = 'third_party/closure_compiler/externs/'
    dep = dep.replace(closure, 'chrome://file_manager_test/%s' % closure, 1)

    # WebUI files (both Polymer and non-Polymer):
    dep = dep.replace('ui/webui/resources/', 'chrome://resources/', 1)

    # Remove the relative because all replaces above map to an absolute path in
    # chrome://* and this URL scheme doesn't allow "..".
    dep = dep.replace('../', '')

    # Find the file being tested eg: element_unittest => element.js
    implementation_file = target_name.replace('_unittest', '.js')

    # If it should use HTMLImport the main element JS file shouldn't be
    # included, instead we <link rel=import> its HTML file which in turn
    # includes the JS file. Note that all other JS deps are included as
    # <script>.
    if dep_type == 'html_import'and dep.endswith(implementation_file):
      dep = dep.replace('.js', '.html')
      yield _IMPORT % (dep)
    elif dep_type == 'js_module':
      yield _JS_MODULE % (dep)
    elif dep_type == 'js_module_register_tests':
      yield _JS_MODULE_REGISTER_TESTS % (dep)
    else:
      # Normal dep, just return the <script src="dep.js">
      yield _CLASSIC_SCRIPT % (dep)


def _process_js_module(input_file, output_filename, mocks, target_name):
  """Generates the HTML for a unittest based on JS Modules.

  Args:
    input_file: The path for the unittest JS module.
    output_filename: The path/filename for HTML to be generated.
    mocks: List of strings, JS file names that will be included in the bottom to
      overwrite JS implementation from deps.
    target_name: Current test target name, used to infer the main Polymer
      element for HTMLImport. element_unitest => element.js/element.html.
  """

  with open(output_filename, 'w') as out:
    out.write(_HTML_FILE)
    for dep in _process_deps(mocks, 'js_module', target_name):
      out.write(dep + '\n')
    for dep in _process_deps([input_file], 'js_module', target_name):
      out.write(dep + '\n')
    for dep in _process_deps([input_file], 'js_module_register_tests',
                             target_name):
      out.write(dep + '\n')


def _process(deps, output_filename, mocks, html_import, target_name):
  """Generates the HTML file with all JS dependencies for JS unittest.

  Args:
    deps: List of strings for each dependency path.
    output_filename: String, HTML file name that will be generated.
    mocks: List of strings, JS file names that will be included in the bottom to
      overwrite JS implementation from deps.
    html_import: Boolean, indicate if HTMLImport should be used for testing
      Polymer elements.
    target_name: Current test target name, used to infer the main Polymer
      element for HTMLImport. element_unitest => element.js/element.html.
  """

  with open(output_filename, 'w') as out:
    out.write(_HTML_FILE)

    # Always add the HTML polyfil and the Polymer config.
    out.write(_HTML_IMPORT_POLYFIL + '\n')
    out.write(_IMPORT % ('chrome://resources/html/polymer.html') + '\n')

    dep_type = 'html_import' if html_import else 'classic_script'
    for dep in _process_deps(mocks, dep_type, target_name):
      out.write(dep + '\n')
    for dep in _process_deps(deps, dep_type, target_name):
      out.write(dep + '\n')

    out.write(_HTML_FOOTER)


def main():
  parser = ArgumentParser()
  parser.add_argument(
      '-s', '--src_path', help='Path to //src/ directory', required=True)
  parser.add_argument(
      '-i',
      '--input',
      help='Input dependency file generated by js_library.py',
      required=True)
  parser.add_argument(
      '-o',
      '--output',
      help='Generated html output with flattened dependencies',
      required=True)
  parser.add_argument(
      '-m',
      '--mocks',
      nargs='*',
      default=[],
      help='List of additional js files to load before others')
  parser.add_argument('-t', '--target_name', help='Test target name')
  parser.add_argument(
      '--html_import',
      action='store_true',
      help='Enable HTMLImports, used for Polymer elements')
  parser.add_argument(
      '--js_module',
      action='store_true',
      help='Enable JS Modules for the unittest file.')
  args = parser.parse_args()

  if args.js_module:
    # Convert from:
    # gen/ui/file_manager/file_manager/common/js/example_unittest.m.js_library
    # To:
    # ui/file_manager/file_manager/common/js/example_unittest.m.js
    path_test_file = args.input.replace('gen/', '', 1)
    path_test_file = path_test_file.replace('.js_library', '.js')
    _process_js_module(path_test_file, args.output, args.mocks,
                              args.target_name)
    return

  # Append closure path to sys.path to be able to import js_unit_test.
  sys.path.append(os.path.join(args.src_path, 'third_party/closure_compiler'))
  from js_binary import CrawlDepsTree

  deps, _ = CrawlDepsTree([args.input])

  return _process(deps, args.output, args.mocks, args.html_import,
                  args.target_name)


if __name__ == '__main__':
  main()

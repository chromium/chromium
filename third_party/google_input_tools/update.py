#!/usr/bin/python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performs pull of google-input-tools from local clone of GitHub repository."""

import json
import logging
import optparse
import os
import re
import shutil
import subprocess

_BASE_REGEX_STRING = r'^\s*goog\.%s\(\s*[\'"](.+)[\'"]\s*\)'
require_regex = re.compile(_BASE_REGEX_STRING % 'require')
provide_regex = re.compile(_BASE_REGEX_STRING % 'provide')

# Entry-points required to build a virtual keyboard.
namespaces = [
    'i18n.input.chrome.inputview.Controller',
    'i18n.input.chrome.inputview.content.compact.letter',
    'i18n.input.chrome.inputview.content.compact.util',
    'i18n.input.chrome.inputview.content.compact.symbol',
    'i18n.input.chrome.inputview.content.compact.more',
    'i18n.input.chrome.inputview.content.compact.numberpad',
    'i18n.input.chrome.inputview.content.ContextlayoutUtil',
    'i18n.input.chrome.inputview.content.util',
    'i18n.input.chrome.inputview.EmojiType',
    'i18n.input.chrome.inputview.layouts.CompactSpaceRow',
    'i18n.input.chrome.inputview.layouts.RowsOf101',
    'i18n.input.chrome.inputview.layouts.RowsOf102',
    'i18n.input.chrome.inputview.layouts.RowsOfCompact',
    'i18n.input.chrome.inputview.layouts.RowsOfJP',
    'i18n.input.chrome.inputview.layouts.RowsOfNumberpad',
    'i18n.input.chrome.inputview.layouts.SpaceRow',
    'i18n.input.chrome.inputview.layouts.util',
    'i18n.input.chrome.inputview.layouts.material.CompactSpaceRow',
    'i18n.input.chrome.inputview.layouts.material.RowsOf101',
    'i18n.input.chrome.inputview.layouts.material.RowsOf102',
    'i18n.input.chrome.inputview.layouts.material.RowsOfCompact',
    'i18n.input.chrome.inputview.layouts.material.RowsOfJP',
    'i18n.input.chrome.inputview.layouts.material.RowsOfNumberpad',
    'i18n.input.chrome.inputview.layouts.material.SpaceRow',
    'i18n.input.chrome.inputview.layouts.material.util',
    'i18n.input.hwt.util'
]

# Any additional required files.
extras = [
    'common.css',
    'emoji.css'
]


def process_file(filename):
  """Extracts provided and required namespaces.

  Description:
    Scans Javascript file for provied and required namespaces.

  Args:
    filename: name of the file to process.

  Returns:
    Pair of lists, where the first list contains namespaces provided by the file
    and the second contains a list of requirements.
  """
  provides = []
  requires = []
  file_handle = open(filename, 'r')
  try:
    for line in file_handle:
      if re.match(require_regex, line):
        requires.append(re.search(require_regex, line).group(1))
      if re.match(provide_regex, line):
        provides.append(re.search(provide_regex, line).group(1))
  finally:
    file_handle.close()
  return provides, requires


def expand_directories(refs):
  """Expands any directory references into inputs.

  Description:
    Looks for any directories in the provided references.  Found directories
    are recursively searched for .js files.

  Args:
    refs: a list of directories.

  Returns:
    Pair of maps, where the first maps each namepace to the filename that
    provides the namespace, and the second maps a filename to prerequisite
    namespaces.
  """
  providers = {}
  requirements = {}
  for ref in refs:
    if os.path.isdir(ref):
      for (root, _, files) in os.walk(ref):
        for name in files:
          if name.endswith('js'):
            filename = os.path.join(root, name)
            provides, requires = process_file(filename)
            for p in provides:
              providers[p] = filename
            requirements[filename] = []
            for r in requires:
              requirements[filename].append(r)
  return providers, requirements


def extract_dependencies(namespace, providers, requirements, dependencies):
  """Recursively extracts all dependencies for a namespace.

  Description:
    Recursively extracts all dependencies for a namespace.

  Args:
    namespace: The namespace to process.
    providers: Mapping of namespace to filename that provides the namespace.
    requirements: Mapping of filename to a list of prerequisite namespaces.
    dependencies: List of files required to build inputview.
  Returns:
  """

  if namespace in providers:
    filename = providers[namespace]
    if filename not in dependencies:
      for ns in requirements[filename]:
        extract_dependencies(ns, providers, requirements, dependencies)
    dependencies.add(filename)


def home_dir():
  """Resolves the user's home directory."""

  return os.path.expanduser('~')


def expand_path_relative_to_home(path):
  """Resolves a path that is relative to the home directory.

  Args:
    path: Relative path.

  Returns:
    Resolved path.
  """

  return os.path.join(os.path.expanduser('~'), path)


def get_google_input_tools_sandbox_from_options(options):
  """Generate the input-input-tools path from the --input flag.

  Args:
    options: Flags to update.py.
  Returns:
    Path to the google-input-tools sandbox.
  """

  path = options.input
  if not path:
    path = expand_path_relative_to_home('google-input-tools')
    print 'Unspecified path for google-input-tools. Defaulting to %s' % path
  return path


def get_closure_library_sandbox_from_options(options):
  """Generate the closure-library path from the --input flag.

  Args:
    options: Flags to update.py.
  Returns:
    Path to the closure-library sandbox.
  """

  path = options.lib
  if not path:
    path = expand_path_relative_to_home('closure-library')
    print 'Unspecified path for closure-library. Defaulting to %s' % path
  return path


def copy_file(source, target):
  """Copies a file from the source to the target location.

  Args:
    source: Path to the source file to copy.
    target: Path to the target location to copy the file.
  """

  if not os.path.exists(os.path.dirname(target)):
    os.makedirs(os.path.dirname(target))
  shutil.copy(source, target)
  # Ensure correct file permissions.
  if target.endswith('py'):
    subprocess.call(['chmod', '+x', target])
  else:
    subprocess.call(['chmod', '-x', target])


def update_file(filename, input_source, closure_source, target_files):
  """Updates files in third_party/google_input_tools.

  Args:
    filename: The file to update.
    input_source: Root of the google_input_tools sandbox.
    closure_source: Root of the closure_library sandbox.
    target_files: List of relative paths to target files.
  """

  target = ''
  if filename.startswith(input_source):
    target = os.path.join('src', filename[len(input_source)+1:])
  elif filename.startswith(closure_source):
    target = os.path.join('third_party/closure_library',
                          filename[len(closure_source)+1:])
  if target:
    copy_file(filename, target)
    target_files.append(os.path.relpath(target, os.getcwd()))


def generate_build_file(target_files):
  """Updates inputview.json.

  Args:
    target_files: List of files required to build inputview.js.
  """

  sorted_files = sorted(target_files)
  with open('inputview.json', 'w') as file_handle:
    json_data = {'inputview_sources': sorted_files}
    json_str = json.dumps(json_data, indent=2, separators=(',', ': '))
    file_handle.write(json_str.replace('\"', '\''))


def copy_dir(input_path, sub_dir):
  """Copies all files in a subdirectory of google-input-tools.

  Description:
    Recursive copy of a directory under google-input-tools.  Used to copy
    localization and resource files.

  Args:
    input_path: Path to the google-input-tools-sandbox.
    sub_dir: Subdirectory to copy within google-input-tools sandbox.
  """
  source_dir = os.path.join(input_path, 'chrome', 'os', sub_dir)
  for (root, _, files) in os.walk(source_dir):
    for name in files:
      filename = os.path.join(root, name)
      relative_path = filename[len(source_dir) + 1:]
      target = os.path.join('src', 'chrome', 'os', sub_dir,
                            relative_path)
      copy_file(filename, target)


def main():
  """The entrypoint for this script."""

  logging.basicConfig(format='update.py: %(message)s', level=logging.INFO)

  usage = 'usage: %prog [options] arg'
  parser = optparse.OptionParser(usage)
  parser.add_option('-i',
                    '--input',
                    dest='input',
                    action='append',
                    help='Path to the google-input-tools sandbox.')
  parser.add_option('-l',
                    '--lib',
                    dest='lib',
                    action='store',
                    help='Path to the closure-library sandbox.')

  (options, _) = parser.parse_args()

  input_path = get_google_input_tools_sandbox_from_options(options)[0]
  closure_library_path = get_closure_library_sandbox_from_options(options)[0]

  if not os.path.isdir(input_path):
    print 'Could not find google-input-tools sandbox.'
    exit(1)
  if not os.path.isdir(closure_library_path):
    print 'Could not find closure-library sandbox.'
    exit(1)

  (providers, requirements) = expand_directories([
      os.path.join(input_path, 'chrome'),
      closure_library_path])

  dependencies = set()
  for name in namespaces:
    extract_dependencies(name, providers, requirements, dependencies)

  target_files = []
  for name in dependencies:
    update_file(name, input_path, closure_library_path, target_files)

  generate_build_file(target_files)

  # Copy resources
  copy_dir(input_path, 'inputview/_locales')
  copy_dir(input_path, 'inputview/images')
  copy_dir(input_path, 'inputview/config')
  copy_dir(input_path, 'inputview/layouts')
  copy_dir(input_path, 'sounds')

  # Copy extra support files.
  for name in extras:
    source = os.path.join(input_path, 'chrome', 'os', 'inputview', name)
    target = os.path.join('src', 'chrome', 'os', 'inputview', name)
    copy_file(source, target)


if __name__ == '__main__':
  main()

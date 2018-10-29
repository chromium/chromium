#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Closure builder for Javascript."""

import argparse
import os
import re
import shlex

_BASE_REGEX_STRING = r'^\s*goog\.%s\(\s*[\'"](.+)[\'"]\s*\)'
require_regex = re.compile(_BASE_REGEX_STRING % 'require')
provide_regex = re.compile(_BASE_REGEX_STRING % 'provide')

base = os.path.join('third_party',
                    'closure_library',
                    'closure',
                    'goog',
                    'base.js')


def process_file(filename):
  """Extracts provided and required namespaces.

  Description:
    Scans Javascript file for provided and required namespaces.

  Args:
    filename: name of the file to process.

  Returns:
    Pair of lists, where the first list contains namespaces provided by the file
    and the second contains a list of requirements.
  """

  provides = []
  requires = []
  with open(filename, 'r') as file_handle:
    for line in file_handle:
      if re.match(require_regex, line):
        requires.append(re.search(require_regex, line).group(1))
      if re.match(provide_regex, line):
        provides.append(re.search(provide_regex, line).group(1))
  return provides, requires


def extract_dependencies(filename, providers, requirements):
  """Extracts provided and required namespaces for a file.

  Description:
    Updates maps for namespace providers and file prerequisites.

  Args:
    filename: Path of the file to process.
    providers: Mapping of namespace to filename that provides the namespace.
    requirements: Mapping of filename to a list of prerequisite namespaces.
  """

  p, r = process_file(filename)

  for name in p:
    providers[name] = filename
  for name in r:
    if filename not in requirements:
      requirements[filename] = []
    requirements[filename].append(name)


def export(target_file, source_filename, providers, requirements, processed):
  """Writes the contents of a file.

  Description:
    Appends the contents of the source file to the target file. In order to
    preserve proper dependencies, each file has its required namespaces
    processed before exporting the source file itself. The set of exported files
    is tracked to guard against multiple exports of the same file. Comments as
    well as 'provide' and 'require' statements are removed during to export to
    reduce file size.

  Args:
    target_file: Handle to target file for export.
    source_filename: Name of the file to export.
    providers: Map of namespace to filename.
    requirements: Map of filename to required namespaces.
    processed: Set of processed files.
  Returns:
  """

  # Filename may have already been processed if it was a requirement of a
  # previous exported file.
  if source_filename in processed:
    return

  # Export requirements before file.
  if source_filename in requirements:
    for namespace in requirements[source_filename]:
      if namespace in providers:
        dependency = providers[namespace]
        if dependency:
          export(target_file, dependency, providers, requirements, processed)

  processed.add(source_filename)

  # Export file
  for name in providers:
    if providers[name] == source_filename:
      target_file.write('// %s%s' % (name, os.linesep))
  source_file = open(source_filename, 'r')
  try:
    comment_block = False
    for line in source_file:
      # Skip require statements.
      if not re.match(require_regex, line):
        formatted = line.rstrip()
        if comment_block:
          # Scan for trailing */ in multi-line comment.
          index = formatted.find('*/')
          if index >= 0:
            formatted = formatted[index + 2:]
            comment_block = False
          else:
            formatted = ''
        # Remove full-line // style comments.
        if formatted.lstrip().startswith('//'):
          formatted = ''
        # Remove /* */ style comments.
        start_comment = formatted.find('/*')
        end_comment = formatted.find('*/')
        while start_comment >= 0:
          if end_comment > start_comment:
            formatted = (formatted[:start_comment]
                         + formatted[end_comment + 2:])
            start_comment = formatted.find('/*')
            end_comment = formatted.find('*/')
          else:
            formatted = formatted[:start_comment]
            comment_block = True
            start_comment = -1
        if formatted.strip():
          target_file.write('%s%s' % (formatted, os.linesep))
  finally:
    source_file.close()
  target_file.write('\n')


def extract_sources(options):
  """Extracts list of sources based on command line options.

  Args:
    options: Parsed command line options.
  Returns:
    List of source files.  If the path option is specified then file paths are
    absolute.  Otherwise, relative paths may be used.
  """

  sources = []
  # Optionally load list of source files from a json file. Useful when the
  # list of files to process is too long for the command line.
  with open(options.sources_list[0], 'r') as f:
    sources = shlex.split(f.read())
  if options.path:
    sources = [os.path.join(options.path, source) for source in sources]
  return sources


def main():
  """The entrypoint for this script."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--sources-list', nargs=1)
  parser.add_argument('--target', nargs=1)
  parser.add_argument('--path', nargs='?')
  options = parser.parse_args()

  sources = extract_sources(options)
  assert sources, 'Missing source files.'

  providers = {}
  requirements = {}
  for filename in sources:
    extract_dependencies(filename, providers, requirements)

  with open(options.target[0], 'w') as target_file:
    target_file.write('var CLOSURE_NO_DEPS=true;%s' % os.linesep)
    processed = set()
    base_path = base
    if options.path:
      base_path = os.path.join(options.path, base_path)
    export(target_file, base_path, providers, requirements, processed)
    for source_filename in sources:
      export(target_file, source_filename, providers, requirements, processed)

if __name__ == '__main__':
  main()

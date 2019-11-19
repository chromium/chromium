# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Identifies Polymer elements that downloaded but not used by Chrome.

Only finds "first-order" unused elements; re-run after removing unused elements
to check if other elements have become unused.
"""

import os
import re
import subprocess
import sys

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

class UnusedElementsDetector(object):
  """Finds unused Polymer elements."""

  # Unused elements to ignore because we plan to use them soon.
  __WHITELIST = (
    # Necessary for closure.
    'polymer-externs',
    # Not used yet. Will be used as part of Polymer 2 migration.
    'polymer2',
    'shadycss',
    # Not used yet. Will be used when pages are moved off of HTML imports.
    'html-imports',
  )

  def __init__(self):
    polymer_dir = os.path.dirname(os.path.realpath(__file__))
    self.__COMPONENTS_DIR = os.path.join(polymer_dir, 'components-chromium')

  @staticmethod
  def __StripHtmlComments(filename):
    """Returns the contents of an HTML file with <!-- --> comments stripped.

    Not a real parser.

    Args:
      filename: The name of the file to read.

    Returns:
      A string consisting of the file contents with comments removed.
    """
    with open(filename) as f:
      return re.sub('<!--.*?-->', '', f.read(), flags=re.MULTILINE | re.DOTALL)

  @staticmethod
  def __StripJsComments(filename):
    """Returns the minified contents of a JavaScript file with comments and
    grit directives removed.

    Args:
      filename: The name of the file to read.

    Returns:
      A string consisting of the minified file contents with comments and grit
      directives removed.
    """
    with open(filename) as f:
      text = f.read()
    text = re.sub('<if .*?>', '', text, flags=re.IGNORECASE)
    text = re.sub('</if>', '', text, flags=re.IGNORECASE)

    return node.RunNode([node_modules.PathToUglify(), filename])

  @staticmethod
  def __StripComments(filename):
    """Returns the contents of a JavaScript or HTML file with comments removed.

    Args:
      filename: The name of the file to read.

    Returns:
      A string consisting of the file contents processed via
      __StripHtmlComments or __StripJsComments.
    """
    if filename.endswith('.html'):
      text = UnusedElementsDetector.__StripHtmlComments(filename)
    elif filename.endswith('.js'):
      text = UnusedElementsDetector.__StripJsComments(filename)
    else:
      assert False, 'Invalid filename: %s' % filename
    return text

  def Run(self):
    """Finds unused Polymer elements and prints a summary."""
    proc = subprocess.Popen(
      ['git', 'rev-parse', '--show-toplevel'],
      stdout=subprocess.PIPE)
    src_dir = proc.stdout.read().strip()

    elements = []
    for name in os.listdir(self.__COMPONENTS_DIR):
      path = os.path.join(self.__COMPONENTS_DIR, name)
      if os.path.isdir(path):
        elements.append(name)

    relevant_src_dirs = (
      os.path.join(src_dir, 'chrome'),
      os.path.join(src_dir, 'ui'),
      os.path.join(src_dir, 'components'),
      self.__COMPONENTS_DIR
    )

    unused_elements = []
    for element in elements:
      if (element not in self.__WHITELIST and
          not self.__IsImported(element, relevant_src_dirs)):
        unused_elements.append(element)

    if unused_elements:
      print 'Found unused elements: %s\nRemove from bower.json and re-run ' \
        'reproduce.sh, or add to whitelist in %s' % (
          ', '.join(unused_elements), os.path.basename(__file__))

  def __IsImported(self, element_dir, dirs):
    """Returns whether the element directory is used in HTML or JavaScript.

    Args:
      element_dir: The name of the element's directory.
      dirs: The directories in which to check for usage.

    Returns:
      True if the element's directory is used in |dirs|.
    """
    for path in dirs:
      # Find an import or script referencing the tag's directory.
      for (dirpath, _, filenames) in os.walk(path):
        # Ignore the element's own files.
        if dirpath.startswith(os.path.join(
            self.__COMPONENTS_DIR, element_dir)):
          continue

        for filename in filenames:
          if not filename.endswith('.html') and not filename.endswith('.js'):
            continue

          with open(os.path.join(dirpath, filename)) as f:
            text = f.read()
          if not re.search('/%s/' % element_dir, text):
            continue

          # Check the file again, ignoring comments (e.g. example imports and
          # scripts).
          if re.search('/%s' % element_dir,
                       self.__StripComments(
                         os.path.join(dirpath, filename))):
            return True
    return False


if __name__ == '__main__':
  UnusedElementsDetector().Run()

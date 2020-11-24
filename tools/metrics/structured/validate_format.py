#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the structured.xml file is well-formatted."""

import os
import re
import sys
from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

STRUCTURED_XML = path_util.GetInputFile(('tools/metrics/structured/'
                                         'structured.xml'))


def checkElementOwners(config, element_tag):
  """Check that every element with the given tag has at least one owner."""
  errors = []

  for node in config.getElementsByTagName(element_tag):
    name = node.getAttribute('name')
    owner_nodes = node.getElementsByTagName('owner')

    # Check <owner> tag is present for each element.
    if not owner_nodes:
      errors.append(
          "<owner> tag is required for %s '%s'." % (element_tag, name))
      continue

    for owner_node in owner_nodes:
      # Check <owner> tag actually has some content.
      if not owner_node.childNodes:
        errors.append("<owner> tag for '%s' should not be empty." % name)
      for email in owner_node.childNodes:
        # Check <owner> tag's content is an email address, not a username.
        if not re.match('^.+@(chromium\.org|google\.com)$', email.data):
          errors.append("<owner> tag for %s '%s' expects a Chromium or "
                        "Google email address, instead found '%s'." %
                        (element_tag, name, email.data.strip()))

  return errors


def checkElementsNotDuplicated(config, element_tag):
  errors = []
  elements = set()

  for node in config.getElementsByTagName(element_tag):
    name = node.getAttribute('name')
    # Check for duplicate names.
    if name in elements:
      errors.append("duplicate %s name '%s'" % (element_tag, name))
    elements.add(name)

  return errors


def main():
  with open(STRUCTURED_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('structured-metrics')
    errors = []

    errors.extend(checkElementOwners(config, 'project'))
    errors.extend(checkElementsNotDuplicated(config, 'project'))
    for project in document.getElementsByTagName('project'):
      errors.extend(checkElementsNotDuplicated(project, 'event'))
      for event in project.getElementsByTagName('event'):
        errors.extend(checkElementsNotDuplicated(event, 'metric'))

    if errors:
      return 'ERRORS:' + ''.join('\n  ' + e for e in errors)

if __name__ == '__main__':
  sys.exit(main())

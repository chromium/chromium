# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates EnterprisePolicies enum in histograms.xml file with policy
definitions read from policy_templates.json.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import os
import re
import sys

from ast import literal_eval
from optparse import OptionParser
from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
from diff_util import PromptUserToAcceptDiff
import path_util

import histogram_paths
import histograms_print_style

ENUMS_PATH = histogram_paths.ENUMS_XML
POLICY_TEMPLATES_PATH = 'components/policy/resources/policy_templates.json'
ENUM_NAME = 'EnterprisePolicies'

class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]


def UpdateHistogramDefinitions(policy_templates, doc):
  """Sets the children of <enum name="EnterprisePolicies" ...> node in |doc| to
  values generated from policy ids contained in |policy_templates|.

  Args:
    policy_templates: A list of dictionaries, defining policies or policy
                      groups. The format is exactly the same as in
                      policy_templates.json file.
    doc: A minidom.Document object representing parsed histogram definitions
         XML file.
  """
  # Find EnterprisePolicies enum.
  for enum_node in doc.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == ENUM_NAME:
        policy_enum_node = enum_node
        break
  else:
    raise UserError('No policy enum node found')

  # Remove existing values.
  while policy_enum_node.hasChildNodes():
    policy_enum_node.removeChild(policy_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(POLICY_TEMPLATES_PATH)
  policy_enum_node.appendChild(doc.createComment(comment))

  # Add values generated from policy templates.
  ordered_policies = [x for x in policy_templates['policy_definitions']
                      if x['type'] != 'group']
  ordered_policies.sort(key=lambda policy: policy['id'])
  for policy in ordered_policies:
    node = doc.createElement('int')
    node.attributes['value'] = str(policy['id'])
    node.attributes['label'] = policy['name']
    policy_enum_node.appendChild(node)


def main():
  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  with open(path_util.GetInputFile(POLICY_TEMPLATES_PATH), 'rb') as f:
    policy_templates = literal_eval(f.read())
  with open(ENUMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  UpdateHistogramDefinitions(policy_templates, histograms_doc)
  new_xml = histograms_print_style.GetPrintStyle().PrettyPrintNode(
      histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(ENUMS_PATH, 'wb') as f:
      f.write(new_xml)


if __name__ == '__main__':
  try:
    main()
  except UserError as e:
    print >>sys.stderr, e.message
    sys.exit(1)

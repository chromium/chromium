# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates EnterprisePolicies enum in histograms.xml file with policy
definitions read from policies.yaml.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import os
import re
import sys

from ast import literal_eval
from optparse import OptionParser
from xml.dom import minidom
sys.path.append(os.path.join(os.path.dirname(__file__), '../../../third_party'))
import pyyaml

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
from diff_util import PromptUserToAcceptDiff
import path_util

import histogram_configuration_model

ENUMS_PATH = 'tools/metrics/histograms/metadata/enterprise/enums.xml'
POLICY_LIST_PATH = 'components/policy/resources/templates/policies.yaml'
POLICIES_ENUM_NAME = 'EnterprisePolicies'

class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]


def UpdatePoliciesHistogramDefinitions(policy_ids, doc):
  """Sets the children of <enum name="EnterprisePolicies" ...> node in |doc| to
  values generated from policy ids contained in |policy_templates|.

  Args:
    policy_ids: A dictionary mapping policy ids to their names.
    doc: A minidom.Document object representing parsed histogram definitions
         XML file.
  """
  # Find EnterprisePolicies enum.
  for enum_node in doc.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == POLICIES_ENUM_NAME:
      policy_enum_node = enum_node
      break
  else:
    raise UserError('No policy enum node found')

  # Remove existing values.
  while policy_enum_node.hasChildNodes():
    policy_enum_node.removeChild(policy_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(POLICY_LIST_PATH)
  policy_enum_node.appendChild(doc.createComment(comment))

  # Add values generated from policy templates.
  ordered_policies = [{
      'id': id,
      'name': name
  } for id, name in policy_ids.items() if name]

  ordered_policies.sort(key=lambda policy: policy['id'])
  for policy in ordered_policies:
    node = doc.createElement('int')
    node.attributes['value'] = str(policy['id'])
    node.attributes['label'] = policy['name']
    policy_enum_node.appendChild(node)


def main():
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  with open(os.path.join(POLICY_LIST_PATH), encoding='utf-8') as f:
    policy_list_content = pyyaml.safe_load(f)

  with open(ENUMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read().decode('utf-8')

  UpdatePoliciesHistogramDefinitions(policy_list_content['policies'],
                                     histograms_doc)
  new_xml = histogram_configuration_model.PrettifyTree(histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(ENUMS_PATH, 'wb') as f:
      f.write(new_xml.encode('utf-8'))


if __name__ == '__main__':
  try:
    main()
  except UserError as e:
    print(e.message, file=sys.stderr)
    sys.exit(1)

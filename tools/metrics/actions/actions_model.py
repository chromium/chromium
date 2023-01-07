# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Model objects for actions.xml contents."""

import os
import sys
import re

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

_OBSOLETE_TYPE = models.TextNodeType('obsolete', single_line=True)
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
_DESCRIPTION_TYPE = models.TextNodeType('description', single_line=True)

# A key for sorting XML nodes by the value of |attribute|.
# Used for sorting tags by their name attribute
_LOWERCASE_FN = lambda attribute: (lambda node: node.get(attribute).lower())

# A constant function as the sorting key for nodes whose orderings should be
# kept as given in the XML file within their parent node.
_KEEP_ORDER = lambda node: 1

_ACTION_TYPE = models.ObjectNodeType(
    'action',
    attributes=[
        ('name', str, None),
        ('not_user_triggered', str, r'^$|^true|false|True|False$'),
    ],
    required_attributes=['name'],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, _KEEP_ORDER),
        (_OWNER_TYPE.tag, _KEEP_ORDER),
        (_DESCRIPTION_TYPE.tag, _KEEP_ORDER),
    ],
    extra_newlines=(1, 1, 1),
    children=[models.ChildType(_OBSOLETE_TYPE.tag,
                               _OBSOLETE_TYPE, multiple=False),
              models.ChildType(_OWNER_TYPE.tag,
                                _OWNER_TYPE, multiple=True),
              models.ChildType(_DESCRIPTION_TYPE.tag,
                               _DESCRIPTION_TYPE, multiple=False),
    ])

_SUFFIX_TYPE = models.ObjectNodeType(
    'suffix',
    attributes=[
        ('name', str, r'^[A-Za-z0-9.-_]*$'),
        ('label', str, None),
    ],
    required_attributes=['name', 'label'],
)

_AFFECTED_ACTION_TYPE = models.ObjectNodeType(
    'affected-action',
    attributes=[
        ('name', str, r'^[A-Za-z0-9.-_]*$'),
    ],
    required_attributes=['name'],
)

_ACTION_SUFFIX_TYPE = models.ObjectNodeType(
    'action-suffix',
    attributes=[
        ('separator', str, r'^$|^[\._]+$'),
        ('ordering', str, r'^$|^suffix$'),
    ],
    required_attributes=['separator'],
    alphabetization=[
        (_SUFFIX_TYPE.tag, _LOWERCASE_FN('name')),
        (_AFFECTED_ACTION_TYPE.tag, _LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_SUFFIX_TYPE.tag, _SUFFIX_TYPE, multiple=True),
        models.ChildType(_AFFECTED_ACTION_TYPE.tag,
                         _AFFECTED_ACTION_TYPE, multiple=True),
    ])

_ACTIONS_TYPE = models.ObjectNodeType(
    'actions',
    alphabetization=[
        (_ACTION_TYPE.tag, _LOWERCASE_FN('name')),
        (_ACTION_SUFFIX_TYPE.tag, lambda n: None),
    ],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_ACTION_TYPE.tag, _ACTION_TYPE, multiple=True),
        models.ChildType(_ACTION_SUFFIX_TYPE.tag,
                         _ACTION_SUFFIX_TYPE, multiple=True),
    ])

ACTION_XML_TYPE = models.DocumentType(_ACTIONS_TYPE)

def PrettifyTree(minidom_doc):
  """Parses the input minidom document and return a pretty-printed
  version.

  Args:
    minidom_doc: A minidom document.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  actions = ACTION_XML_TYPE.Parse(minidom_doc)
  return ACTION_XML_TYPE.PrettyPrint(actions)

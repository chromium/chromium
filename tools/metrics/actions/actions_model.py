# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Model objects for actions.xml contents."""

import os
import re
import sys
from typing import Any, List

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


def _NaturalSortByName(node: Any) -> List[Any]:
  """Natural-sorting XML nodes.
  Used for sorting nodes by their name attribute in a way that humans
  understand. i.e. "suffix11" should come after "suffix2
  See: https://blog.codinghorror.com/sorting-for-humans-natural-sort-order/
  """
  name = node.get('name').lower()
  convert = lambda text: int(text) if text.isdigit() else text
  return [convert(c) for c in re.split('([0-9]+)', name)]


# Action Suffix Types (for pre-migration)

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

# Patterned Action Types (for post-migration)
_VARIANT_TYPE = models.ObjectNodeType(
    'variant',
    attributes=[
        ('name', str, None),
        ('summary', str, None),
    ],
    required_attributes=['name'],
)

_TOKEN_TYPE = models.ObjectNodeType('token',
                                    attributes=[('key', str, None),
                                                ('variants', str, None)],
                                    required_attributes=['key'],
                                    alphabetization=[(_VARIANT_TYPE.tag,
                                                      _NaturalSortByName)],
                                    children=[
                                        models.ChildType(_VARIANT_TYPE.tag,
                                                         _VARIANT_TYPE,
                                                         multiple=True),
                                    ])

_ACTION_TYPE = models.ObjectNodeType('action',
                                     attributes=[
                                         ('name', str, None),
                                         ('not_user_triggered', str, None),
                                     ],
                                     required_attributes=['name'],
                                     alphabetization=[
                                         (_OBSOLETE_TYPE.tag, _KEEP_ORDER),
                                         (_OWNER_TYPE.tag, _KEEP_ORDER),
                                         (_DESCRIPTION_TYPE.tag, _KEEP_ORDER),
                                         (_TOKEN_TYPE.tag, _KEEP_ORDER),
                                     ],
                                     extra_newlines=(1, 1, 1),
                                     children=[
                                         models.ChildType(_OBSOLETE_TYPE.tag,
                                                          _OBSOLETE_TYPE,
                                                          multiple=False),
                                         models.ChildType(_OWNER_TYPE.tag,
                                                          _OWNER_TYPE,
                                                          multiple=True),
                                         models.ChildType(_DESCRIPTION_TYPE.tag,
                                                          _DESCRIPTION_TYPE,
                                                          multiple=False),
                                         models.ChildType(_TOKEN_TYPE.tag,
                                                          _TOKEN_TYPE,
                                                          multiple=True),
                                     ])

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
                         _AFFECTED_ACTION_TYPE,
                         multiple=True),
    ])

_VARIANTS_TYPE = models.ObjectNodeType('variants',
                                       attributes=[
                                           ('name', str, None),
                                       ],
                                       required_attributes=['name'],
                                       alphabetization=[(_VARIANT_TYPE.tag,
                                                         _NaturalSortByName)],
                                       extra_newlines=(1, 1, 1),
                                       children=[
                                           models.ChildType(_VARIANT_TYPE.tag,
                                                            _VARIANT_TYPE,
                                                            multiple=True),
                                       ])

_ACTIONS_TYPE = models.ObjectNodeType(
    'actions',
    alphabetization=[
        (_VARIANTS_TYPE.tag, _LOWERCASE_FN('name')),
        (_ACTION_TYPE.tag, _LOWERCASE_FN('name')),
        (_ACTION_SUFFIX_TYPE.tag, lambda n: None),
    ],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_VARIANTS_TYPE.tag, _VARIANTS_TYPE, multiple=True),
        models.ChildType(_ACTION_TYPE.tag, _ACTION_TYPE, multiple=True),
        models.ChildType(_ACTION_SUFFIX_TYPE.tag,
                         _ACTION_SUFFIX_TYPE,
                         multiple=True),
    ])

# TODO: Remove suffix from model after migration to patterned actions.
# crbug.com/374120501
ACTION_XML_TYPE = models.DocumentType(_ACTIONS_TYPE)


def PrettifyTree(input_tree):
  """Parses the XML tree and returns a pretty-printed version.

   Args:
    input_tree: A tree representation of the XML, which might take the
    form of an ET tree or minidom doc.

   Returns:
    A pretty-printed xml string, or None if the config contains errors.
   """
  actions = ACTION_XML_TYPE.Parse(input_tree)
  return ACTION_XML_TYPE.PrettyPrint(actions)

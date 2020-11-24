# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

# Helpers

# A key for sorting XML nodes by the value of |attribute|.
_LOWERCASE_FN = lambda attribute: (lambda node: node.get(attribute).lower())
# A constant function as the sorting key for nodes whose orderings should be
# kept as given in the XML file within their parent node.
_KEEP_ORDER = lambda node: 1

# Model definitions

_OBSOLETE_TYPE = models.TextNodeType('obsolete')
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
_ID_TYPE = models.TextNodeType('id', single_line=True)
_SUMMARY_TYPE = models.TextNodeType('summary')

_METRIC_TYPE =  models.ObjectNodeType(
    'metric',
    attributes=[
      ('name', unicode, r'^[A-Za-z0-9_.]+$'),
      ('kind', unicode, r'^(?i)(|hashed-string|int)$'),
    ],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, lambda _: 1),
        (_SUMMARY_TYPE.tag, lambda _: 2),
    ],
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
    ])

_EVENT_TYPE = models.ObjectNodeType('event',
    attributes=[('name', unicode, r'^[A-Z][A-Za-z0-9.]*$'),],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, lambda _: 1),
        (_OWNER_TYPE.tag, lambda _: 2),
        (_SUMMARY_TYPE.tag, lambda _: 3),
        (_METRIC_TYPE.tag,
         _LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(_METRIC_TYPE.tag, _METRIC_TYPE, multiple=True),
    ])

_PROJECT_TYPE = models.ObjectNodeType('project',
    attributes=[
        ('name', unicode, r'^[A-Z][A-Za-z0-9.]*$'),
    ],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, lambda _: 1),
        (_OWNER_TYPE.tag, lambda _: 2),
        (_ID_TYPE.tag, lambda _: 3),
        (_SUMMARY_TYPE.tag, lambda _: 4),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_ID_TYPE.tag, _ID_TYPE, multiple=False),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(_EVENT_TYPE.tag, _EVENT_TYPE, multiple=True),
    ])

CONFIGURATION_TYPE = models.ObjectNodeType(
    'structured-metrics',
    alphabetization=[(_PROJECT_TYPE.tag, _LOWERCASE_FN('name'))],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_PROJECT_TYPE.tag, _PROJECT_TYPE, multiple=True),
    ])

XML_TYPE = models.DocumentType(CONFIGURATION_TYPE)

def PrettifyXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = XML_TYPE.Parse(original_xml)
  return XML_TYPE.PrettyPrint(config)

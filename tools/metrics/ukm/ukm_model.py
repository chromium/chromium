# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# """Model objects for ukm.xml contents."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

# Model definitions for ukm.xml content
_OBSOLETE_TYPE = models.TextNodeType('obsolete')
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
_SUMMARY_TYPE = models.TextNodeType('summary')

# A key for sorting XML nodes by the value of |attribute|.
_LOWERCASE_FN = lambda attribute: (lambda node: node.get(attribute).lower())
# A constant function as the sorting key for nodes whose orderings should be
# kept as given in the XML file within their parent node.
_KEEP_ORDER = lambda node: 1

_ENUMERATION_TYPE = models.ObjectNodeType(
    'enumeration',
    attributes=[],
    single_line=True)

_QUANTILES_TYPE = models.ObjectNodeType(
    'quantiles',
    attributes=[
      ('type', str, None),
    ],
    single_line=True)

_INDEX_TYPE = models.ObjectNodeType(
    'index',
    attributes=[
      ('fields', str, None),
    ],
    single_line=True)

_STATISTICS_TYPE = models.ObjectNodeType(
    'statistics',
    attributes=[
        ('export', str, r'(?i)^(|true|false)$'),
    ],
    children=[
        models.ChildType(_QUANTILES_TYPE.tag, _QUANTILES_TYPE, multiple=False),
        models.ChildType(_ENUMERATION_TYPE.tag,
                         _ENUMERATION_TYPE,
                         multiple=False),
    ])

_HISTORY_TYPE =  models.ObjectNodeType(
    'history',
    attributes=[],
    alphabetization=[
        (_INDEX_TYPE.tag, _LOWERCASE_FN('fields')),
        (_STATISTICS_TYPE.tag, _KEEP_ORDER),
    ],
    children=[
        models.ChildType(_INDEX_TYPE.tag, _INDEX_TYPE, multiple=True),
        models.ChildType(_STATISTICS_TYPE.tag, _STATISTICS_TYPE, multiple=True),
    ])

_AGGREGATION_TYPE =  models.ObjectNodeType(
    'aggregation',
    attributes=[],
    children=[
        models.ChildType(_HISTORY_TYPE.tag, _HISTORY_TYPE, multiple=False),
    ])

_METRIC_TYPE =  models.ObjectNodeType(
    'metric',
    attributes=[
      ('name', str, r'^[A-Za-z][A-Za-z0-9_.]*$'),
      ('semantic_type', str, None),
      ('enum', str, None),
    ],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, _KEEP_ORDER),
        (_OWNER_TYPE.tag, _KEEP_ORDER),
        (_SUMMARY_TYPE.tag, _KEEP_ORDER),
        (_AGGREGATION_TYPE.tag, _KEEP_ORDER),
    ],
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(
            _AGGREGATION_TYPE.tag, _AGGREGATION_TYPE, multiple=True),
    ])

_EVENT_TYPE = models.ObjectNodeType(
    'event',
    attributes=[
        ('name', str, r'^[A-Za-z][A-Za-z0-9.]*$'),
        ('singular', str, r'(?i)^(|true|false)$'),
        # This event will be omitted from the generated readable_event.proto
        # file if skip_proto_reason is a non-empty string.
        ('skip_proto_reason', str, None),
    ],
    alphabetization=[
        (_OBSOLETE_TYPE.tag, _KEEP_ORDER),
        (_OWNER_TYPE.tag, _KEEP_ORDER),
        (_SUMMARY_TYPE.tag, _KEEP_ORDER),
        (_METRIC_TYPE.tag, _LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(_OBSOLETE_TYPE.tag, _OBSOLETE_TYPE, multiple=False),
        models.ChildType(_OWNER_TYPE.tag, _OWNER_TYPE, multiple=True),
        models.ChildType(_SUMMARY_TYPE.tag, _SUMMARY_TYPE, multiple=False),
        models.ChildType(_METRIC_TYPE.tag, _METRIC_TYPE, multiple=True),
    ])

_UKM_CONFIGURATION_TYPE = models.ObjectNodeType(
    'ukm-configuration',
    alphabetization=[(_EVENT_TYPE.tag, _LOWERCASE_FN('name'))],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_EVENT_TYPE.tag, _EVENT_TYPE, multiple=True),
    ])

UKM_XML_TYPE = models.DocumentType(_UKM_CONFIGURATION_TYPE)

def PrettifyXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = UKM_XML_TYPE.Parse(original_xml)
  return UKM_XML_TYPE.PrettyPrint(config)


def IsNotObsolete(node):
  """Checks if the given node contains a child <obsolete> node."""
  return _OBSOLETE_TYPE.tag not in node

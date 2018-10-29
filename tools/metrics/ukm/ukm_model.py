# Copyright 2017 The Chromium Authors. All rights reserved.
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

_LOWERCASE_NAME_FN = lambda n: n.attributes['name'].value.lower()

_METRIC_TYPE =  models.ObjectNodeType(
    'metric',
    attributes=[
      ('name', unicode),
      ('semantic_type', unicode),
    ],
    children=[
        models.ChildType('obsolete', _OBSOLETE_TYPE, False),
        models.ChildType('owners', _OWNER_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_EVENT_TYPE =  models.ObjectNodeType(
    'event',
    alphabetization=[('metric', _LOWERCASE_NAME_FN)],
    attributes=[('name', unicode), ('singular', bool)],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType('obsolete', _OBSOLETE_TYPE, False),
        models.ChildType('owners', _OWNER_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('metrics', _METRIC_TYPE, True),
    ])

_UKM_CONFIGURATION_TYPE = models.ObjectNodeType(
    'ukm-configuration',
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType('events', _EVENT_TYPE, True),
    ])

UKM_XML_TYPE = models.DocumentType(_UKM_CONFIGURATION_TYPE)

def UpdateXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = UKM_XML_TYPE.Parse(original_xml)

  return UKM_XML_TYPE.PrettyPrint(config)

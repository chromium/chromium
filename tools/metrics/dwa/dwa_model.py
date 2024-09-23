# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# """Model objects for dwa.xml contents."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models
import model_shared

_METRIC_TYPE = models.ObjectNodeType(
    'metric',
    attributes=[
      ('name', str, r'^[A-Za-z][A-Za-z0-9_.]*$'),
      ('semantic_type', str, None),
      ('enum', str, None),
      ('expires_after', str, None),
    ],
    alphabetization=[
        (model_shared._SUMMARY_TYPE.tag, model_shared._KEEP_ORDER),
    ],
    children=[
        models.ChildType(
          model_shared._SUMMARY_TYPE.tag, model_shared._SUMMARY_TYPE, \
            multiple=False),
    ])

_STUDY_TYPE = models.ObjectNodeType('study',
                                    attributes=[
                                        ('name', str,
                                         r'^[A-Za-z][A-Za-z0-9_.]*$'),
                                        ('semantic_type', str, None),
                                        ('enum', str, None),
                                        ('expires_after', str, None),
                                    ])

_EVENT_TYPE = models.ObjectNodeType(
    'event',
    attributes=[
        ('name', str, r'^[A-Za-z][A-Za-z0-9.]*$'),
        ('singular', str, r'(?i)^(|true|false)$'),
    ],
    alphabetization=[
        (model_shared._OWNER_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._SUMMARY_TYPE.tag, model_shared._KEEP_ORDER),
        (_STUDY_TYPE.tag, model_shared._LOWERCASE_FN('name')),
        (_METRIC_TYPE.tag, model_shared._LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(model_shared._OWNER_TYPE.tag,
                         model_shared._OWNER_TYPE,
                         multiple=True),
        models.ChildType(model_shared._SUMMARY_TYPE.tag,
                         model_shared._SUMMARY_TYPE,
                         multiple=False),
        models.ChildType(_STUDY_TYPE.tag, _STUDY_TYPE, multiple=True),
        models.ChildType(_METRIC_TYPE.tag, _METRIC_TYPE, multiple=True),
    ])

_DWA_CONFIGURATION_TYPE = models.ObjectNodeType(
    'dwa-configuration',
    alphabetization=[(_EVENT_TYPE.tag, model_shared._LOWERCASE_FN('name'))],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_EVENT_TYPE.tag, _EVENT_TYPE, multiple=True),
    ])

DWA_XML_TYPE = models.DocumentType(_DWA_CONFIGURATION_TYPE)


def PrettifyXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = DWA_XML_TYPE.Parse(original_xml)
  return DWA_XML_TYPE.PrettyPrint(config)

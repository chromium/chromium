# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared model objects and utils for Private Metrics."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models
import model_shared

METRIC_TYPE = models.ObjectNodeType(
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

STUDY_TYPE = models.ObjectNodeType('study',
                                   attributes=[
                                       ('name', str,
                                        r'^[A-Za-z][A-Za-z0-9_.-]*$'),
                                       ('semantic_type', str, None),
                                       ('enum', str, None),
                                       ('expires_after', str, None),
                                   ])

EVENT_TYPE = models.ObjectNodeType(
    'event',
    attributes=[
        ('name', str, r'^[A-Za-z][A-Za-z0-9.]*$'),
        ('singular', str, r'(?i)^(|true|false)$'),
    ],
    alphabetization=[
        (model_shared._OWNER_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._SUMMARY_TYPE.tag, model_shared._KEEP_ORDER),
        (STUDY_TYPE.tag, model_shared._LOWERCASE_FN('name')),
        (METRIC_TYPE.tag, model_shared._LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(model_shared._OWNER_TYPE.tag,
                         model_shared._OWNER_TYPE,
                         multiple=True),
        models.ChildType(model_shared._SUMMARY_TYPE.tag,
                         model_shared._SUMMARY_TYPE,
                         multiple=False),
        models.ChildType(STUDY_TYPE.tag, STUDY_TYPE, multiple=True),
        models.ChildType(METRIC_TYPE.tag, METRIC_TYPE, multiple=True),
    ])


def create_event_based_document_type(tag):
  """Create new document type for an event-based Private Metrics configuration.

  Currently, that includes DKM and DWA. See go/pmc-dkm for details.

  Args:
    tag: Tag of the root node

  Returns:
    Document type.
  """
  # TODO(crbug.com/411370913): Add public-facing docs and link them here
  configuration_type = models.ObjectNodeType(
      tag,
      alphabetization=[(EVENT_TYPE.tag, model_shared._LOWERCASE_FN('name'))],
      extra_newlines=(2, 1, 1),
      indent=False,
      children=[
          models.ChildType(EVENT_TYPE.tag, EVENT_TYPE, multiple=True),
      ])
  return models.DocumentType(configuration_type)

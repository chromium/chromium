# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# """Model objects for ukm.xml contents."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models
import model_shared


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
        (_INDEX_TYPE.tag, model_shared._LOWERCASE_FN('fields')),
        (_STATISTICS_TYPE.tag, model_shared._KEEP_ORDER),
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
        (model_shared._OBSOLETE_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._OWNER_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._SUMMARY_TYPE.tag, model_shared._KEEP_ORDER),
        (_AGGREGATION_TYPE.tag, model_shared._KEEP_ORDER),
    ],
    children=[
        models.ChildType(
          model_shared._OBSOLETE_TYPE.tag, model_shared._OBSOLETE_TYPE, \
            multiple=False),
        models.ChildType(
          model_shared._OWNER_TYPE.tag, model_shared._OWNER_TYPE, \
            multiple=True),
        models.ChildType(
          model_shared._SUMMARY_TYPE.tag, model_shared._SUMMARY_TYPE, \
            multiple=False),
        models.ChildType(
          _AGGREGATION_TYPE.tag, _AGGREGATION_TYPE, \
            multiple=True),
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
        (model_shared._OBSOLETE_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._OWNER_TYPE.tag, model_shared._KEEP_ORDER),
        (model_shared._SUMMARY_TYPE.tag, model_shared._KEEP_ORDER),
        (_METRIC_TYPE.tag, model_shared._LOWERCASE_FN('name')),
    ],
    extra_newlines=(1, 1, 1),
    children=[
        models.ChildType(
          model_shared._OBSOLETE_TYPE.tag, model_shared._OBSOLETE_TYPE, \
            multiple=False),
        models.ChildType(
          model_shared._OWNER_TYPE.tag, model_shared._OWNER_TYPE, \
            multiple=True),
        models.ChildType(
          model_shared._SUMMARY_TYPE.tag, model_shared._SUMMARY_TYPE, \
            multiple=False),
        models.ChildType(
          _METRIC_TYPE.tag, _METRIC_TYPE, \
            multiple=True),
    ])

_UKM_CONFIGURATION_TYPE = models.ObjectNodeType(
    'ukm-configuration',
    alphabetization=[(_EVENT_TYPE.tag, model_shared._LOWERCASE_FN('name'))],
    extra_newlines=(2, 1, 1),
    indent=False,
    children=[
        models.ChildType(_EVENT_TYPE.tag, _EVENT_TYPE, multiple=True),
    ])

UKM_XML_TYPE = models.DocumentType(_UKM_CONFIGURATION_TYPE)


def PrettifyXmlAndTrimObsolete(original_xml: str) -> str:
  """Parses the original XML and returns a pretty-printed version.

  This also removes any events and metrics tagged as <obsolete>, and prints
  a warning if applicable.

  Args:
    original_xml: A string containing the original XML file contents.

  Returns:
    A pretty-printed XML string, or None if the config contains errors.
  """
  config = UKM_XML_TYPE.Parse(original_xml)
  event_tag = _EVENT_TYPE.tag
  metric_tag = _METRIC_TYPE.tag
  all_event_names = {event['name'] for event in config[event_tag]}
  config[event_tag] = list(filter(IsNotObsolete, config[event_tag]))
  non_obsolete_event_names = {event['name'] for event in config[event_tag]}
  if all_event_names != non_obsolete_event_names:
    print(f'Events {str(all_event_names - non_obsolete_event_names)} are'
          ' tagged as <obsolete>; deleting their definition as per'
          ' go/ukm-api#obsolete-events.\n')
  for event in config[event_tag]:
    all_metrics_names = {metric['name'] for metric in event[metric_tag]}
    event[metric_tag] = list(filter(IsNotObsolete, event[metric_tag]))
    non_obsolete_metrics_names = {
        metric['name']
        for metric in event[metric_tag]
    }
    if all_metrics_names != non_obsolete_metrics_names:
      event_name = event['name']
      print(f'Under the event {event_name!r}, metrics'
            f' {str(all_metrics_names - non_obsolete_metrics_names)} are'
            ' tagged as <obsolete>; deleting their definition as per'
            ' go/ukm-api#obsolete-events.\n')

  return UKM_XML_TYPE.PrettyPrint(config)


def IsNotObsolete(node):
  """Checks if the given node contains a child <obsolete> node."""
  return model_shared._OBSOLETE_TYPE.tag not in node

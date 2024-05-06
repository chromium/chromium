# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'histograms'))
import extract_histograms
import histogram_paths
import merge_xml


LOCAL_METRIC_RE = re.compile(r'metrics\.([^,]+)')
INVALID_LOCAL_METRIC_FIELD_ERROR = (
  'Invalid index field specification in ukm metric %(event)s:%(metric)s, the '
  'following metrics are used as index fields but are not configured to '
  'support it: [%(invalid_metrics)s]\n\n'
  'See https://chromium.googlesource.com/chromium/src.git/+/main/services/'
  'metrics/ukm_api.md#aggregation-by-metrics-in-the-same-event for '
  'instructions on how to configure them.')


def _isMetricValidAsIndexField(metric_node):
  """Checks if a given metric node can be used as a field in an index tag.

  Has the following requirements:
    * 'history' is the only aggregation target (no others are considered)
    * there will be at most 1 'aggregation', 1 'history', and 1 'statistic'
      element in a metric element
    * enumerations are the only metric types that are valid

  Args:
    metric_node: A metric node to check.

  Returns: True or False, depending on whethere the given node is valid as an
    index field.
  """
  aggregation_nodes = metric_node.getElementsByTagName('aggregation')
  if aggregation_nodes.length != 1:
    return False

  history_nodes = aggregation_nodes[0].getElementsByTagName('history')
  if history_nodes.length != 1:
    return False

  statistic_nodes = history_nodes[0].getElementsByTagName('statistics')
  if statistic_nodes.length != 1:
    return False

  # Only enumeration type metrics are supported as index fields.
  enumeration_nodes = statistic_nodes[0].getElementsByTagName('enumeration')
  return bool(enumeration_nodes)


def _getIndexFields(metric_node):
  """Get a list of fields from index node descendents of a metric_node."""
  aggregation_nodes = metric_node.getElementsByTagName('aggregation')
  if not aggregation_nodes:
    return []

  history_nodes = aggregation_nodes[0].getElementsByTagName('history')
  if not history_nodes:
    return []

  index_nodes = history_nodes[0].getElementsByTagName('index')
  if not index_nodes:
    return []

  return [index_node.getAttribute('fields') for index_node in index_nodes]


def _getLocalMetricIndexFields(metric_node):
  """Gets a set of metric names being used as local-metric index fields."""
  index_fields = _getIndexFields(metric_node)
  local_metric_fields = set()
  for fields in index_fields:
    local_metric_fields.update(LOCAL_METRIC_RE.findall(fields))
  return local_metric_fields


class UkmXmlValidation(object):
  """Validations for the content of ukm.xml."""

  def __init__(self, ukm_config):
    """Attributes:

    config: A XML minidom Element representing the root node of the UKM config
        tree.
    """
    self.config = ukm_config

  def checkEventsHaveOwners(self):
    """Check that every event in the config has at least one owner."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      owner_nodes = event_node.getElementsByTagName('owner')

      # Check <owner> tag is present for each event.
      if not owner_nodes:
        errors.append("<owner> tag is required for event '%s'." % event_name)
        continue

      for owner_node in owner_nodes:
        # Check <owner> tag actually has some content.
        if not owner_node.childNodes:
          errors.append(
              "<owner> tag for event '%s' should not be empty." % event_name)
          continue

        for email in owner_node.childNodes:
          # Check <owner> tag's content is an email address, not a username.
          if not ('@chromium.org' in email.data or '@google.com' in email.data):
            errors.append("<owner> tag for event '%s' expects a Chromium or "
                          "Google email address." % event_name)

    isSuccess = not errors

    return (isSuccess, errors)

  def checkMetricTypeIsSpecified(self):
    """Check each metric is either specified with an enum or a unit."""
    errors = []
    warnings = []

    enum_tree = merge_xml.MergeFiles(histogram_paths.ENUMS_XMLS)
    enums, _ = extract_histograms.ExtractEnumsFromXmlTree(enum_tree)

    for event_node in self.config.getElementsByTagName('event'):
      for metric_node in event_node.getElementsByTagName('metric'):
        if metric_node.hasAttribute('enum'):
          enum_name = metric_node.getAttribute('enum');
          # Check if the enum is defined in enums.xml.
          if enum_name not in enums:
            errors.append("Unknown enum %s in ukm metric %s:%s." %
                          (enum_name, event_node.getAttribute('name'),
                          metric_node.getAttribute('name')))
        elif not metric_node.hasAttribute('unit'):
          warnings.append("Warning: Neither \'enum\' or \'unit\' is specified "
                          "for ukm metric %s:%s."
                          % (event_node.getAttribute('name'),
                             metric_node.getAttribute('name')))

    isSuccess = not errors
    return (isSuccess, errors, warnings)

  def checkLocalMetricIsAggregated(self):
    """Checks that index fields don't list invalid metrics."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      metric_nodes = event_node.getElementsByTagName('metric')
      valid_index_field_metrics = {node.getAttribute('name')
                                   for node in metric_nodes
                                   if _isMetricValidAsIndexField(node)}
      for metric_node in metric_nodes:
        local_metric_index_fields = _getLocalMetricIndexFields(metric_node)
        invalid_metrics = local_metric_index_fields - valid_index_field_metrics
        if invalid_metrics:
          event_name = event_node.getAttribute('name')
          metric_name = metric_node.getAttribute('name')
          invalid_metrics_string = ', '.join(sorted(invalid_metrics))
          errors.append(INVALID_LOCAL_METRIC_FIELD_ERROR %(
                          {'event': event_name, 'metric': metric_name,
                           'invalid_metrics': invalid_metrics_string}))

    is_success = not errors
    return (is_success, errors)

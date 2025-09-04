# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import xml.dom.minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'ukm'))
import xml_validations

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'histograms'))
import extract_histograms
import histogram_paths
import merge_xml


class DwaXmlValidation(xml_validations.UkmXmlValidation):
  """Validations for the content of dwa.xml."""

  def __init__(self, dwa_config: xml.dom.minidom.Element) -> None:
    """Attributes:

    config: A XML minidom Element representing the root node of the DWA config
        tree.
    """
    super().__init__(dwa_config)

  def checkMetricTypeIsSpecified(self):
    """Checks each metric is either specified with an enum or a unit."""
    errors = []

    enum_tree = merge_xml.MergeFiles(histogram_paths.ENUMS_XMLS)
    enums, _ = extract_histograms.ExtractEnumsFromXmlTree(enum_tree)

    for event_node in self.config.getElementsByTagName('event'):
      for metric_node in event_node.getElementsByTagName('metric'):
        if metric_node.hasAttribute('enum'):
          enum_name = metric_node.getAttribute('enum')
          # Check if the enum is defined in enums.xml.
          if enum_name not in enums:
            errors.append("Unknown enum %s in dwa metric %s:%s." %
                          (enum_name, event_node.getAttribute('name'),
                           metric_node.getAttribute('name')))

    is_success = not errors

    return (is_success, errors)

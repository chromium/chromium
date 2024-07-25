# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for populating enums with ukm events."""

from collections import namedtuple
import os
import sys
import xml.dom.minidom

import xml_utils

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import codegen_shared


EventDetails = namedtuple("EventDetails", "name hash is_obsolete")


def _GetEventDetails(event):
  """Returns a simple struct containing the event details.

  Args:
    event: An event description as defined in ukm.xml.

  Returns:
    A struct containing the event name, truncated hash, and whether the event is
    considered obsolete.
  """
  name = event.getAttribute('name')
  # The value is UKM event name hash truncated to 31 bits. This is recorded in
  # https://cs.chromium.org/chromium/src/components/ukm/ukm_recorder_impl.cc?q=LogEventHashasUmaHistogram
  hash = codegen_shared.HashName(name) & 0x7fffffff

  def _HasDirectObsoleteTag(node):
    return any(
        isinstance(child, xml.dom.minidom.Element)
        and child.tagName == 'obsolete' for child in node.childNodes)

  # The UKM event is considered obsolete if the event itself is marked as
  # obsolete with a tag or all of its metrics are marked as obsolete.
  is_event_obsolete = _HasDirectObsoleteTag(event)
  are_all_metrics_obsolete = all(
      _HasDirectObsoleteTag(metric)
      for metric in event.getElementsByTagName('metric'))

  return EventDetails(name=name,
                      hash=hash,
                      is_obsolete=is_event_obsolete or are_all_metrics_obsolete)


def PopulateEnumWithUkmEvents(doc, enum, ukm_events):
  """Populates the enum node with a list of ukm events.

  Args:
    doc: The document to create the node in.
    enum: The enum node needed to be populated.
    ukm_events: A list of ukm event nodes.
  """
  event_details = [_GetEventDetails(event) for event in ukm_events]
  event_details.sort(key=lambda event: event.hash)

  for event in event_details:
    node = doc.createElement('int')
    node.attributes['value'] = str(event.hash)
    label = event.name
    # If the event is obsolete, mark it in the int's label.
    if event.is_obsolete:
      label += ' (Obsolete)'
    node.attributes['label'] = label
    enum.appendChild(node)


def PopulateEnumsWithUkmEvents(doc, enums, ukm_events):
  """Populates enum nodes in the enums with a list of ukm events

  Args:
    doc: The document to create the node in.
    enums: The enums node to be iterated.
    ukm_events: A list of ukm event nodes.
  """
  for enum in xml_utils.IterElementsWithTag(enums, 'enum', 1):
    # We only special case 'UkmEventNameHash' currently.
    if enum.getAttribute('name') == 'UkmEventNameHash':
      PopulateEnumWithUkmEvents(doc, enum, ukm_events)

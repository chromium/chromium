# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for populating enums with ukm events."""

import collections
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.codegen_shared as codegen_shared
import chromium_src.tools.metrics.common.xml_utils as xml_utils

EventDetails = collections.namedtuple('EventDetails', 'name hash is_obsolete')


def _GetEventDetails(event: ET.Element) -> EventDetails:
  """Returns a simple struct containing the event details.

  Args:
    event: An event description as defined in ukm.xml.

  Returns:
    A struct containing the event name, truncated hash, and whether the event is
    considered obsolete.
  """
  name = event.get('name')
  if name is None:
    raise ValueError('Event is missing name attribute.')
  # The value is UKM event name hash truncated to 31 bits. This is recorded in
  # https://cs.chromium.org/chromium/src/components/ukm/ukm_recorder_impl.cc?q=LogEventHashasUmaHistogram
  name_hash = codegen_shared.HashName(name) & 0x7FFFFFFF

  def _HasDirectObsoleteTag(node):
    return any(child.tag == 'obsolete' for child in node)

  # The UKM event is considered obsolete if the event itself is marked as
  # obsolete with a tag or all of its metrics are marked as obsolete.
  is_event_obsolete = _HasDirectObsoleteTag(event)
  are_all_metrics_obsolete = all(
    _HasDirectObsoleteTag(metric) for metric in event.findall('metric')
  )

  return EventDetails(
    name=name,
    hash=name_hash,
    is_obsolete=is_event_obsolete or are_all_metrics_obsolete,
  )


def PopulateEnumWithUkmEvents(enum: ET.Element, ukm_events: list[ET.Element]):
  """Populates the enum node with a list of ukm events.

  Args:
    enum: The enum node needed to be populated.
    ukm_events: A list of ukm event nodes.
  """
  event_details = [_GetEventDetails(event) for event in ukm_events]
  event_details.sort(key=lambda event: event.hash)

  for event in event_details:
    node = ET.Element('int')
    node.set('value', str(event.hash))
    label = event.name
    # If the event is obsolete, mark it in the int's label.
    if event.is_obsolete:
      label += ' (Obsolete)'
    node.set('label', label)
    enum.append(node)


def PopulateEnumsWithUkmEvents(enums: ET.Element, ukm_events: list[ET.Element]):
  """Populates enum nodes in the enums with a list of ukm events.

  Args:
    enums: The enums node to be iterated.
    ukm_events: A list of ukm event nodes.
  """
  for enum in xml_utils.IterElementsWithTag(enums, 'enum', 1):
    # We only special case 'UkmEventNameHash' currently.
    if enum.get('name') == 'UkmEventNameHash':
      PopulateEnumWithUkmEvents(enum, ukm_events)

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that structured.xml is well-structured."""

from collections import Counter
from model import _METRIC_TYPE
from model import _EVENT_TYPE
from model import _PROJECT_TYPE


def projectsHaveRequiredFields(data):
  """Check that projects have all fields required for compilation."""
  valid_id_types = {'none', 'per-project', 'uma'}

  for project in data[_PROJECT_TYPE.tag]:
    if 'name' not in project:
      raise Exception('Structured metrics project has no name')

    if 'id' not in project:
      raise Exception("Structured metrics project '{}' has no id field.".format(
          project['name']))
    if project['id']['text'] not in valid_id_types:
      raise Exception(
          "Structured metrics project '{}' has invalid id field.".format(
              project['name']))

  name_counts = Counter(
      project['name']
      for project in data[_PROJECT_TYPE.tag])
  for name, count in name_counts.items():
    if count != 1:
      raise Exception(
          "Structured metrics projects have duplicate name '{}'.".format(name))


def metricNamesUniqueWithinEvent(data):
  """Check that no two metrics within an event have the same name."""
  for project in data[_PROJECT_TYPE.tag]:
    for event in project[_EVENT_TYPE.tag]:
      name_counts = Counter(
        metric['name']
        for metric in event[_METRIC_TYPE.tag])

      for name, count in name_counts.items():
        if count != 1:
          raise Exception(("Structured metrics project '{}' and event '{}' has "
                           "duplicated metric name '{}'.").format(
                             project['name'], event['name'], name))


def eventNamesUniqueWithinProject(data):
  """Check that no two events in a project have the same name."""
  for project in data[_PROJECT_TYPE.tag]:
    name_counts = Counter(
      event['name']
      for event in project[_EVENT_TYPE.tag])

    for name, count in name_counts.items():
      if count != 1:
        raise Exception(
            "Structured metrics project '{}' has events with duplicate "
            "name '{}'.".format(project['name'], name))


def projectNamesUnique(data):
  """Check that no two projects have the same name."""
  name_counts = Counter(
      project['name']
      for project in data[_PROJECT_TYPE.tag])

  for name, count in name_counts.items():
    if count != 1:
      raise Exception(
          "Structured metrics projects have duplicate name '{}'.".format(name))


def validate(data):
  projectsHaveRequiredFields(data)
  projectNamesUnique(data)
  eventNamesUniqueWithinProject(data)
  metricNamesUniqueWithinEvent(data)

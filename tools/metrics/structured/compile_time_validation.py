# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that structured.xml is well-structured."""

from collections import Counter
from model import _METRIC_TYPE
from model import _EVENT_TYPE, _EVENTS_TYPE
from model import _PROJECT_TYPE, _PROJECTS_TYPE


def projectsHaveRequiredFields(data):
  """Check that projects have all fields required for compilation."""
  for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag]:
    if 'name' not in project:
      raise Exception('Structured metrics project has no name')
    if 'id' not in project:
      raise Exception("Structured metrics project '{}' has no id field.".format(
          project['name']))

  name_counts = Counter(
      project['name']
      for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag])
  for name, count in name_counts.items():
    if count != 1:
      raise Exception(
          "Structured metrics projects have duplicate name '{}'.".format(name))


def eventsReferenceValidProjects(data):
  """Check that any project referenced by an event exists."""
  projects = {
      project['name']: project
      for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag]
  }
  for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag]:
    project_name = event.get('project')
    project = projects.get(project_name)
    if project is None and project_name is not None:
      raise Exception(("Structured metrics event '{}' references "
                       "nonexistent project '{}'.").format(
                           event['name'], project_name))


def metricNamesUniqueWithinEvent(data):
  """Check that no two metrics within an event have the same name."""
  for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag]:
    name_counts = Counter(metric['name'] for metric in event[_METRIC_TYPE.tag])
    for name, count in name_counts.items():
      if count != 1:
        raise Exception(("Structured metrics event '{}' has duplicated metric "
                         "name '{}'.").format(event['name'], name))


def eventNamesUniqueWithinProject(data):
  """Check that no two events in a project have the same name."""
  name_counts = Counter((event['project'], event['name'])
                        for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag])
  for (project, name), count in name_counts.items():
    if count != 1:
      raise Exception(
          "Structured metrics project '{}' has events with duplicate "
          "name '{}'.".format(project, name))


def projectNamesUnique(data):
  """Check that no two projects have the same name."""
  name_counts = Counter(
      project['name']
      for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag])
  for name, count in name_counts.items():
    if count != 1:
      raise Exception(
          "Structured metrics projects have duplicate name '{}'.".format(name))


def validate(data):
  projectsHaveRequiredFields(data)
  eventsReferenceValidProjects(data)
  metricNamesUniqueWithinEvent(data)
  eventNamesUniqueWithinProject(data)
  projectNamesUnique(data)

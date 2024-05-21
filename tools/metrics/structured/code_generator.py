# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Abstract class definition of Code Generation from structured.xml
into definitions for different targets."""

from abc import ABC, abstractmethod
from codegen_util import Util

# Default target if not explicitly specified in the XML.
DEFAULT_TARGET = "chromium"


class EventTemplateBase(ABC):
  """EventTemplate is a base abstract class, which contains the common
    abstract methods and common constructor to generate code for
    structured events."""

  def __init__(self, model, dirname, basename, file_template, project_template,
               enum_template, event_template, metric_template):
    self.model = model
    self.dirname = dirname
    self.basename = basename
    self.file_template = file_template
    self.project_template = project_template
    self.event_template = event_template
    self.enum_template = enum_template
    self.metric_template = metric_template

  @abstractmethod
  def write_file(self):
    pass


class ProjectInfoBase(ABC):
  """Codegen-related info about a Structured Metrics project.
  Constructor takes in a set of target_names in order to determine
  whether a project should have its code generated."""

  def __init__(self, project, target_names):
    self.name = Util.sanitize_name(project.name)
    self.namespace = Util.camel_to_snake(self.name)
    self.name_hash = Util.hash_name(self.name)
    self.events = project.events
    self.enums = project.enums
    self.targets = project.targets
    self.target_names = target_names
    self.platform = project.platform

    # Set ID type.
    if project.id == 'uma':
      self.id_type = 'kUmaId'
    elif project.id == 'per-project':
      self.id_type = 'kProjectId'
    elif project.id == 'none':
      self.id_type = 'kUnidentified'
    else:
      raise ValueError('Invalid id type.')

    # Set ID scope
    if project.scope == 'profile':
      self.id_scope = 'kPerProfile'
    elif project.scope == 'device':
      self.id_scope = 'kPerDevice'
    else:
      raise ValueError('Invalid id scope.')

    # Set event type. This is inferred by checking all metrics within the
    # project. If any of a project's metrics is a raw string, then its events
    # are considered raw string events, even if they also contain non-strings.
    self.event_type = 'REGULAR'
    for event in project.events:
      for metric in event.metrics:
        if metric.type == 'raw-string':
          self.event_type = 'RAW_STRING'
          break

    # Check if event is part of an event sequence. Note that this goes after the
    # raw string check since the type has higher priority.
    if project.is_event_sequence_project:
      self.is_event_sequence = 'true'
      self.event_type = 'SEQUENCE'
    else:
      self.is_event_sequence = 'false'

    self.key_rotation_period = project.key_rotation_period

    if len(self.targets) == 0:
      self.targets.add(DEFAULT_TARGET)

    # Check if the Project should be generated for the given target.
    self.should_codegen = False
    for target in self.targets:
      if target in self.target_names:
        self.should_codegen = True


class EventInfoBase(ABC):
  """Codegen-related info about a Structured Metrics event."""

  def __init__(self, event, project_info):
    self.name = Util.sanitize_name(event.name)
    self.project_name = project_info.name
    self.platform = project_info.platform
    self.name_hash = Util.event_name_hash(project_info.name, self.name,
                                          project_info.platform)
    self.is_event_sequence = project_info.is_event_sequence
    self.force_record = str(event.force_record).lower()
    self.metrics = event.metrics


class MetricInfoBase(ABC):
  """Codegen-related info about a Structured Metrics metric."""

  def __init__(self, metric):
    self.name = Util.sanitize_name(metric.name)
    self.hash = Util.hash_name(metric.name)
    self.is_enum = metric.is_enum

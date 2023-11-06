# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Objects for describing template code to be generated from structured.xml."""

import hashlib
import os
import re
import struct

import templates_validator as validator_tmpl


class Util:
  """Helpers for generating C++."""

  @staticmethod
  def sanitize_name(name):
    return re.sub('[^0-9a-zA-Z_]', '_', name)

  @staticmethod
  def camel_to_snake(name):
    pat = '((?<=[a-z0-9])[A-Z]|(?!^)[A-Z](?=[a-z]))'
    return re.sub(pat, r'_\1', name).lower()

  @staticmethod
  def hash_name(name):
    # This must match the hash function in chromium's
    # //base/metrics/metric_hashes.cc. >Q means 8 bytes, big endian.
    name = name.encode('utf-8')
    md5 = hashlib.md5(name)
    return struct.unpack('>Q', md5.digest()[:8])[0]

  @staticmethod
  def event_name_hash(project_name, event_name):
    """Make the name hash for an event.

    This gets uploaded in the StructuredEventProto.event_name_hash field. It is
    the sole means of recording which event from structured.xml a
    StructuredEventProto instance represents.

    To avoid naming collisions, it must contain three pieces of information:
     - the name of the event itself
     - the name of the event's project, to avoid collisions with events of the
       same name in other projects
     - an identifier that this comes from chromium, to avoid collisions with
       events and projects of the same name defined in cros's structured.xml

    This must use sanitized names for the project and event.
    """
    event_name = Util.sanitize_name(event_name)
    project_name = Util.sanitize_name(project_name)
    # TODO(crbug.com/1148168): Once the minimum python version is 3.6+, rewrite
    # this .format and others using f-strings.
    return Util.hash_name('chrome::{}::{}'.format(project_name, event_name))


class FileInfo:
  """Codegen-related info about a file."""

  def __init__(self, dirname, basename):
    self.dirname = dirname
    self.basename = basename
    self.rootname = os.path.splitext(self.basename)[0]
    self.filepath = os.path.join(dirname, basename)


    # This takes the last three components of the filepath for use in the
    # header guard, ie. METRICS_STRUCTURED_STRUCTURED_EVENTS_H_
    relative_path = os.sep.join(self.filepath.split(os.sep)[-3:])
    self.guard_path = Util.sanitize_name(relative_path).upper()


class ProjectInfo:
  """Codegen-related info about a project."""

  def __init__(self, project):
    self.name = Util.sanitize_name(project.name)
    self.namespace = Util.camel_to_snake(self.name)
    self.name_hash = Util.hash_name(self.name)
    self.validator = '{}ProjectValidator'.format(self.name)
    self.validator_snake_name = Util.camel_to_snake(self.validator)
    self.events = project.events

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

  def build_validator_code(self) -> str:
    event_infos = (EventInfo(event, self) for event in self.events)

    # Generate map entries.
    validator_map_str = ';\n  '.join(
        'event_validators_.emplace("{}", std::make_unique<{}>())'.format(
            event_info.name, event_info.validator_name)
        for event_info in event_infos)

    return validator_tmpl.IMPL_PROJECT_VALIDATOR_TEMPLATE.format(
        project=self, event_validator_map=validator_map_str)


class EventInfo:
  """Codegen-related info about an event."""

  def __init__(self, event, project_info):
    self.name = Util.sanitize_name(event.name)
    self.name_hash = Util.event_name_hash(project_info.name, self.name)
    self.validator_name = '{}EventValidator'.format(self.name)
    self.validator_snake_name = Util.camel_to_snake(self.validator_name)
    self.project_name = project_info.name
    self.is_event_sequence = project_info.is_event_sequence
    self.force_record = str(event.force_record).lower()
    self.metrics = event.metrics

  def build_metric_hash_map(self) -> str:
    metric_infos = (MetricInfo(metric) for metric in self.metrics)
    return ',\n  '.join(
        '{{\"{}\", {{ Event::MetricType::{}, UINT64_C({})}}}}'.format(
            metric_info.name, metric_info.type_enum, metric_info.hash)
        for metric_info in metric_infos)

  def build_validator_code(self) -> str:
    metric_hash_map = "  return absl::nullopt;" if len(
        self.metrics) == 0 else self.build_metric_hash_map()
    return validator_tmpl.IMPL_EVENT_VALIDATOR_TEMPLATE.format(
        event=self, metric_hash_map=self.build_metric_hash_map())


class MetricInfo:
  """Codegen-related info about a metric."""

  def __init__(self, metric):
    self.name = Util.sanitize_name(metric.name)
    self.hash = Util.hash_name(metric.name)

    if metric.type == 'hmac-string':
      self.type = 'std::string&'
      self.setter = 'AddHmacMetric'
      self.type_enum = 'kHmac'
      self.base_value = 'base::Value(value)'
    elif metric.type == 'int':
      self.type = 'int64_t'
      self.setter = 'AddIntMetric'
      self.type_enum = 'kLong'
      self.base_value = 'base::Value(base::NumberToString(value))'
    elif metric.type == 'raw-string':
      self.type = 'std::string&'
      self.setter = 'AddRawStringMetric'
      self.type_enum = 'kRawString'
      self.base_value = 'base::Value(value)'
    elif metric.type == 'double':
      self.type = 'double'
      self.setter = 'AddDoubleMetric'
      self.type_enum = 'kDouble'
      self.base_value = 'base::Value(value)'
    else:
      raise ValueError('Invalid metric type.')


class Template:
  """Template for producing code from structured.xml."""

  def __init__(self, model, dirname, basename, file_template, project_template,
               event_template, metric_template):
    self.model = model
    self.dirname = dirname
    self.basename = basename
    self.file_template = file_template
    self.project_template = project_template
    self.event_template = event_template
    self.metric_template = metric_template

  def write_file(self):
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info):
    project_code = ''.join(
        self._stamp_project(file_info, p) for p in self.model.projects)

    return self.file_template.format(file=file_info, project_code=project_code)

  def _stamp_project(self, file_info, project):
    project_info = ProjectInfo(project)
    event_code = ''.join(
        self._stamp_event(file_info, project_info, event)
        for event in project.events)
    return self.project_template.format(file=file_info,
                                        project=project_info,
                                        event_code=event_code)

  def _stamp_event(self, file_info, project_info, event):
    event_info = EventInfo(event, project_info)
    metric_code = ''.join(
        self._stamp_metric(file_info, event_info, metric)
        for metric in event.metrics)
    return self.event_template.format(file=file_info,
                                      project=project_info,
                                      event=event_info,
                                      metric_code=metric_code)

  def _stamp_metric(self, file_info, event_info, metric):
    return self.metric_template.format(file=file_info,
                                       event=event_info,
                                       metric=MetricInfo(metric))


class ValidatorHeaderTemplate:
  """Template for generating header validator code from structured.xml."""

  def __init__(self, dirname, basename):
    self.dirname = dirname
    self.basename = basename

  def write_file(self) -> None:
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info) -> str:
    return validator_tmpl.HEADER_FILE_TEMPLATE.format(file=file_info)


class ValidatorImplTemplate:
  """Template for generating implementation validator code from structured.xml.

  The generated file will store a static map containing all the validators
  mapped by event name. All validators are initialized statically.

  Almost everything is generated in an anonymous namespace as the generated map
  should not be exposed. The generated code will be in the following order:

    1) EventValidator class implementation.
    2) Project map initialization mapping event name to corresponding
    EventValidator.
    3) Project class implementation.
    4) Map initialization mapping project name to ProjectValidator.
  """

  def __init__(self, structured_model, dirname, basename):
    self.structured_model = structured_model
    self.dirname = dirname
    self.basename = basename
    self.projects = self.structured_model.projects

  def write_file(self) -> None:
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info) -> str:
    event_code = []
    project_event_maps = []
    project_code = []

    for project in self.projects:
      project_info = ProjectInfo(project)
      event_infos = (EventInfo(event, project_info) for event in project.events)
      project_event_code = '\n'.join(event_info.build_validator_code()
                                     for event_info in event_infos)

      event_code.append(project_event_code)
      project_code.append(project_info.build_validator_code())

    # Turn all lists into strings.
    events_code_str = ''.join(event_code)
    project_event_maps_str = '\n'.join(project_event_maps)
    project_code_str = ''.join(project_code)

    return validator_tmpl.IMPL_FILE_TEMPLATE.format(
        file=file_info,
        projects_code=project_code_str,
        event_code=events_code_str,
        project_event_maps=project_event_maps_str,
        project_map=self._build_project_map())

  def _build_project_map(self) -> str:
    project_infos = (ProjectInfo(project) for project in self.projects)
    return ';\n  '.join(
        'validators_.emplace("{}", std::make_unique<{}>())'.format(
            project.name, project.validator) for project in project_infos)

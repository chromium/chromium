# -*- coding: utf-8 -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Objects for describing template code to be generated from structured.xml."""

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.structured.templates_validator as validator_tmpl
from chromium_src.tools.metrics.structured.codegen_util import FileInfo
from chromium_src.tools.metrics.structured.code_generator import (
  EventTemplateBase,
  ProjectInfoBase,
  EventInfoBase,
  MetricInfoBase,
)

CHROMIUM_TARGET = 'chromium'


class ProjectInfoCpp(ProjectInfoBase):
  """Codegen-related info about a project in C++."""

  def __init__(self, project):
    super().__init__(project, CHROMIUM_TARGET)


class EventInfoCpp(EventInfoBase):
  """Codegen-related info about an event in C++."""


class MetricInfoCpp(MetricInfoBase):
  """Codegen-related info about a metric in C++."""

  def __init__(self, metric):
    super().__init__(metric)

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
    elif metric.type == 'int-array':
      # todo(b/341807121): support int array in chromium.
      self.type_enum = ''
    else:
      if self.is_enum:
        self.type = metric.type
        self.setter = f'AddEnumMetric<{self.type}>'
        self.type_enum = 'kInt'
        self.base_value = 'base::Value((int) value)'
      else:
        raise ValueError('Invalid metric type.')


class TemplateCpp(EventTemplateBase):
  """Template for producing C++ code from structured.xml."""

  def __init__(
    self,
    model,
    dirname,
    basename,
    file_template,
    project_template,
    enum_template,
    event_template,
    metric_template,
    header,
  ):
    super().__init__(
      model,
      dirname,
      basename,
      file_template,
      project_template,
      enum_template,
      event_template,
      metric_template,
    )
    self.header = header

  def write_file(self):
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info):
    project_code = ''.join(
      self._stamp_project(file_info, p) for p in self.model.projects
    )

    return self.file_template.format(file=file_info, project_code=project_code)

  def _stamp_project(self, file_info, project):
    project_info = ProjectInfoCpp(project)
    event_code = ''.join(
      self._stamp_event(file_info, project_info, event)
      for event in project.events
    )
    if self.header:
      enum_code = '\n\n'.join(
        [self._stamp_enum(enum) for enum in project.enums]
      )
      return self.project_template.format(
        file=file_info,
        project=project_info,
        enum_code=enum_code,
        event_code=event_code,
      )
    return self.project_template.format(
      file=file_info, project=project_info, event_code=event_code
    )

  def _stamp_event(self, file_info, project_info, event):
    event_info = EventInfoCpp(event, project_info)
    metric_code = ''
    if project_info.platform != 'cros':
      metric_code = ''.join(
        self._stamp_metric(file_info, event_info, metric)
        for metric in event.metrics
      )
    return self.event_template.format(
      file=file_info,
      project=project_info,
      event=event_info,
      metric_code=metric_code,
    )

  def _stamp_enum(self, enum):
    variants = ',\n'.join(
      ['{v.name} = {v.value}'.format(v=v) for v in enum.variants]
    )
    return self.enum_template.format(enum=enum, variants=variants)

  def _stamp_metric(self, file_info, event_info, metric):
    return self.metric_template.format(
      file=file_info, event=event_info, metric=MetricInfoCpp(metric)
    )


class ValidatorHeaderTemplate:
  """Template for generating header validator code from structured.xml."""

  def __init__(self, dirname, basename):
    self.dirname = dirname
    self.basename = basename

  def write_file(self) -> None:
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info: FileInfo) -> str:
    return validator_tmpl.HEADER_FILE_TEMPLATE.format(file=file_info)


class ValidatorImplTemplate:
  """Template for generating implementation validator code from structured.xml.

  Generates constexpr flat tables (kStrings, kMetrics, kEvents, kProjects) in
  an anonymous namespace sorted by name for binary-search lookup with zero
  heap allocations or dynamic relocations.
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

  @staticmethod
  def _format_array_init(entries: list[str]) -> str:
    if not entries:
      return '{}'
    body = ',\n      '.join(entries)
    return '{{\n      ' + body + ',\n  }}'

  def _stamp_file(self, file_info: FileInfo) -> str:
    project_infos = sorted(
      (ProjectInfoCpp(project) for project in self.projects),
      key=lambda p: p.name.encode('utf-8'),
    )
    if len({p.name for p in project_infos}) != len(project_infos):
      raise ValueError('Duplicate sanitized project names in validator model.')

    unique_names: set[str] = set()
    structured_tree = []
    for project_info in project_infos:
      unique_names.add(project_info.name)
      event_infos = sorted(
        (EventInfoCpp(event, project_info) for event in project_info.events),
        key=lambda e: e.name.encode('utf-8'),
      )
      if len({e.name for e in event_infos}) != len(event_infos):
        raise ValueError(
          f'Duplicate sanitized event names in project {project_info.name}.'
        )
      events_with_metrics = []
      for event_info in event_infos:
        unique_names.add(event_info.name)
        metric_infos = sorted(
          (MetricInfoCpp(metric) for metric in event_info.metrics),
          key=lambda m: m.name.encode('utf-8'),
        )
        if len({m.name for m in metric_infos}) != len(metric_infos):
          raise ValueError(
            f'Duplicate sanitized metric names in event {event_info.name}.'
          )
        for metric_info in metric_infos:
          unique_names.add(metric_info.name)
        events_with_metrics.append((event_info, metric_infos))
      structured_tree.append((project_info, events_with_metrics))

    string_blob = ''
    string_offsets: dict[str, int] = {}
    for name in sorted(unique_names, key=lambda s: (-len(s), s)):
      if len(name) > 0xFF:
        raise ValueError(f'Identifier exceeds uint8_t length limit: {name}')
      idx = string_blob.find(name)
      if idx == -1:
        idx = len(string_blob)
        string_blob += name
      string_offsets[name] = idx

    if len(string_blob) > 0xFFFF:
      raise ValueError(
        f'Validator string table ({len(string_blob)}B) exceeds uint16_t limit.'
      )

    metric_entries: list[str] = []
    event_entries: list[str] = []
    project_entries: list[str] = []

    for project_info, events_with_metrics in structured_tree:
      events_offset = len(event_entries)
      events_count = len(events_with_metrics)
      if events_offset > 0xFFFF or events_count > 0xFFFF:
        raise ValueError('Event table exceeds uint16_t index limit.')

      for event_info, metric_infos in events_with_metrics:
        metrics_offset = len(metric_entries)
        metrics_count = len(metric_infos)
        if metrics_offset > 0xFFFF:
          raise ValueError('Metric table exceeds uint16_t offset limit.')
        if metrics_count > 0xFF:
          raise ValueError(
            f'Event {event_info.name} exceeds uint8_t metric count limit.'
          )

        can_validate_metrics = event_info.platform != 'cros'
        for metric_info in metric_infos:
          type_enum = metric_info.type_enum
          if not type_enum:
            if can_validate_metrics:
              raise ValueError(
                f'Unsupported metric type in {event_info.name}:'
                f' {metric_info.name}'
              )
            type_enum = 'kHmac'
          metric_entries.append(
            f'{{UINT64_C({metric_info.hash}), '
            f'{string_offsets[metric_info.name]}, '
            f'{len(metric_info.name)}, '
            f'Event::MetricType::{type_enum}}}'
          )

        can_validate_str = 'true' if can_validate_metrics else 'false'
        event_entries.append(
          f'EventValidator(UINT64_C({event_info.name_hash}), '
          f'{string_offsets[event_info.name]}, '
          f'{len(event_info.name)}, '
          f'{metrics_offset}, '
          f'{metrics_count}, '
          f'{event_info.force_record}, '
          f'{can_validate_str})'
        )

      if project_info.key_rotation_period > 0xFFFF:
        raise ValueError(
          f'Key rotation period exceeds uint16_t in {project_info.name}.'
        )
      project_entries.append(
        f'ProjectValidator(UINT64_C({project_info.name_hash}), '
        f'StructuredEventProto_EventType_{project_info.event_type}, '
        f'IdType::{project_info.id_type}, '
        f'IdScope::{project_info.id_scope}, '
        f'{project_info.key_rotation_period}, '
        f'{string_offsets[project_info.name]}, '
        f'{len(project_info.name)}, '
        f'{events_offset}, '
        f'{events_count})'
      )

    if string_blob:
      chunks = [
        f'    "{string_blob[i : i + 68]}"'
        for i in range(0, len(string_blob), 68)
      ]
      string_table_literal = '\n'.join(chunks)
    else:
      string_table_literal = '    ""'

    return validator_tmpl.IMPL_FILE_TEMPLATE.format(
      file=file_info,
      string_table_literal=string_table_literal,
      string_table_len=len(string_blob),
      num_metrics=len(metric_entries),
      metrics_init=self._format_array_init(metric_entries),
      num_events=len(event_entries),
      events_init=self._format_array_init(event_entries),
      num_projects=len(project_entries),
      projects_init=self._format_array_init(project_entries),
    )

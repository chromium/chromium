# -*- coding: utf-8 -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Objects for describing template code to be generated from structured.xml."""

import templates_validator as validator_tmpl
from codegen_util import FileInfo, Util
from code_generator import (EventTemplateBase, ProjectInfoBase,
  EventInfoBase, MetricInfoBase)

CHROMIUM_TARGET="chromium"

class ProjectInfoCpp(ProjectInfoBase):
  """Codegen-related info about a project in C++."""

  def __init__(self, project):
    super().__init__(project, CHROMIUM_TARGET)
    self.validator = '{}ProjectValidator'.format(self.name)
    self.validator_snake_name = Util.camel_to_snake(self.validator)

  def build_validator_code(self) -> str:
    event_infos = list(EventInfoCpp(event, self) for event in self.events)

    # Generate map entries.
    validator_map_str = ';\n  '.join(
        'event_validators_.emplace("{}", std::make_unique<{}>())'.format(
            event_info.name, event_info.validator_name)
        for event_info in event_infos)

    event_name_map_str = ';\n  '.join(
        'event_name_map_.emplace(UINT64_C({}), "{}")'.format(
            event_info.name_hash, event_info.name)
        for event_info in event_infos)

    return validator_tmpl.IMPL_PROJECT_VALIDATOR_TEMPLATE.format(
        project=self,
        event_validator_map=validator_map_str,
        event_name_map=event_name_map_str)


class EventInfoCpp(EventInfoBase):
  """Codegen-related info about an event in C++."""

  def __init__(self, event, project_info):
    super().__init__(event, project_info)
    self.validator_name = '{}_{}EventValidator'.format(self.project_name,
                                                       self.name)
    self.validator_snake_name = Util.camel_to_snake(self.validator_name)

  def build_metric_hash_map(self) -> str:
    if self.platform == 'cros':
      return ''
    metric_infos = (MetricInfoCpp(metric) for metric in self.metrics)
    return ',\n  '.join(
        '{{\"{}\", {{ Event::MetricType::{}, UINT64_C({})}}}}'.format(
            metric_info.name, metric_info.type_enum, metric_info.hash)
        for metric_info in metric_infos)

  def build_metric_name_map(self) -> str:
    metric_infos = (MetricInfoCpp(metric) for metric in self.metrics)
    return ',\n  '.join(
        '{{ UINT64_C({}), "{}" }}'.format(metric_info.hash, metric_info.name)
        for metric_info in metric_infos)

  def build_validator_code(self) -> str:
    return validator_tmpl.IMPL_EVENT_VALIDATOR_TEMPLATE.format(
        event=self,
        metric_hash_map=self.build_metric_hash_map(),
        metrics_name_map=self.build_metric_name_map())


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

  def __init__(self, model, dirname, basename, file_template, project_template,
               enum_template, event_template, metric_template, header):
    super().__init__(model, dirname, basename, file_template, project_template,
               enum_template, event_template, metric_template)
    self.header = header

  def write_file(self):
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file(file_info))

  def _stamp_file(self, file_info):
    project_code = ''.join(
        self._stamp_project(file_info, p) for p in self.model.projects)

    return self.file_template.format(file=file_info, project_code=project_code)

  def _stamp_project(self, file_info, project):
    project_info = ProjectInfoCpp(project)
    event_code = ''.join(
        self._stamp_event(file_info, project_info, event)
        for event in project.events)
    if self.header:
      enum_code = '\n\n'.join(
          [self._stamp_enum(enum) for enum in project.enums])
      return self.project_template.format(file=file_info,
                                          project=project_info,
                                          enum_code=enum_code,
                                          event_code=event_code)
    return self.project_template.format(file=file_info,
                                        project=project_info,
                                        event_code=event_code)

  def _stamp_event(self, file_info, project_info, event):
    event_info = EventInfoCpp(event, project_info)
    metric_code = ''
    if project_info.platform != 'cros':
      metric_code = ''.join(
          self._stamp_metric(file_info, event_info, metric)
          for metric in event.metrics)
    return self.event_template.format(file=file_info,
                                      project=project_info,
                                      event=event_info,
                                      metric_code=metric_code)

  def _stamp_enum(self, enum):
    variants = ',\n'.join(
        ['{v.name} = {v.value}'.format(v=v) for v in enum.variants])
    return self.enum_template.format(enum=enum, variants=variants)

  def _stamp_metric(self, file_info, event_info, metric):
    return self.metric_template.format(file=file_info,
                                       event=event_info,
                                       metric=MetricInfoCpp(metric))


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
      project_info = ProjectInfoCpp(project)
      event_infos = (EventInfoCpp(event, project_info)
        for event in project.events)
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
        project_map=self._build_project_map(),
        name_map=self._build_name_map())

  def _build_project_map(self) -> str:
    project_infos = (ProjectInfoCpp(project) for project in self.projects)
    return ';\n  '.join(
        'validators_.emplace("{}", std::make_unique<{}>())'.format(
            project.name, project.validator) for project in project_infos)

  def _build_name_map(self):
    project_infos = (ProjectInfoCpp(project) for project in self.projects)
    return ';\n  '.join('project_name_map_.emplace(UINT64_C({}), "{}")'.format(
        project.name_hash, project.name) for project in project_infos)

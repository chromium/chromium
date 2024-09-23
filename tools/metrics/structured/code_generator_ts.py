# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from codegen_util import FileInfo, Util
from code_generator import (EventTemplateBase, ProjectInfoBase, EventInfoBase,
                            MetricInfoBase)


class ProjectInfoTs(ProjectInfoBase):
  """Codegen-related info about a project in Typescript."""

  def __init__(self, project):
    super().__init__(project, "webui")


class EventInfoTs(EventInfoBase):
  """Codegen-related info about an event in Typescript."""

  def __init__(self, event, project_info):
    super().__init__(event, project_info)

    if self.is_event_sequence == 'true':
      self.systemUptime = (
          '{microseconds: BigInt(Math.floor(Date.now() * 1000))}')
    else:
      self.systemUptime = 'null'


class MetricInfoTs(MetricInfoBase):
  """Codegen-related info about a metric in Typescript."""

  def __init__(self, metric, project_info, event_info):
    super().__init__(metric)

    self.project_name = project_info.name
    self.event_name = event_info.name

    if metric.type == 'hmac-string':
      self.ts_type = 'number'
      self.type_enum = 'hmacValue'
    elif metric.type == 'int':
      self.ts_type = 'bigint'
      self.type_enum = 'longValue'
    elif metric.type == 'raw-string':
      self.ts_type = 'string'
      self.type_enum = 'rawStrValue'
    elif metric.type == 'double':
      self.ts_type = 'number'
      self.type_enum = 'doubleValue'
    elif metric.type == 'int-array':
      # todo(b/341807121): support int array in Typescript.
      self.type_enum = ''
    else:
      if self.is_enum:
        self.ts_type = project_info.name + "_" + metric.type
        self.type_enum = 'intValue'
      else:
        raise ValueError('Invalid metric type.')


class TemplateTypescript(EventTemplateBase):
  """Template for producing Typescript code from structured.xml, to
  be used in WebUI pages."""

  def __init__(self, model, dirname, basename, file_template, project_template,
               enum_template, event_template, metric_template,
               metric_field_template, metric_build_code_template):
    super().__init__(model, dirname, basename, file_template, project_template,
                     enum_template, event_template, metric_template)
    self.metric_field_template = metric_field_template
    self.metric_build_code_template = metric_build_code_template

  def write_file(self):
    file_info = FileInfo(self.dirname, self.basename)
    with open(file_info.filepath, 'w') as f:
      f.write(self._stamp_file())

  def _stamp_file(self):
    project_code = "".join(
        self._stamp_project(project) for project in self.model.projects)
    return self.file_template.format(project_code=project_code)

  def _stamp_project(self, project):
    project_info = ProjectInfoTs(project)
    if project_info.should_codegen:
      event_code = ''.join(
          self._stamp_event(project_info, event) for event in project.events)
      enum_code = '\n\n'.join(
          self._stamp_enum(project_info, enum) for enum in project.enums)
      return self.project_template.format(project=project,
                                          enum_code=enum_code,
                                          event_code=event_code)
    return ""

  def _stamp_event(self, project_info, event):
    event_info = EventInfoTs(event, project_info)
    metric_code = ''.join(
        self._stamp_metric(project_info, event_info, metric)
        for metric in event.metrics)
    metric_fields = ''.join(
        self._stamp_metric_fields(project_info, event_info, metric)
        for metric in event.metrics)
    metric_build_code = ''.join(
        self._stamp_metric_build_code(project_info, event_info, metric)
        for metric in event.metrics)
    return self.event_template.format(event=event_info,
                                      metric_fields=metric_fields,
                                      metric_code=metric_code,
                                      metric_build_code=metric_build_code)

  def _stamp_metric(self, project_info, event_info, metric):
    metric_info = MetricInfoTs(metric, project_info, event_info)
    return self.metric_template.format(metric=metric_info)

  def _stamp_metric_fields(self, project_info, event_info, metric):
    metric_info = MetricInfoTs(metric, project_info, event_info)
    return self.metric_field_template.format(metric=metric_info)

  def _stamp_metric_build_code(self, project_info, event_info, metric):
    metric_info = MetricInfoTs(metric, project_info, event_info)
    return self.metric_build_code_template.format(metric=metric_info)

  def _stamp_enum(self, project_info, enum):
    variants = ',\n'.join(
        ['{v.name} = {v.value}'.format(v=v) for v in enum.variants])
    return self.enum_template.format(project_info=project_info,
                                     enum=enum,
                                     variants=variants)

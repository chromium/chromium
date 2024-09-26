# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Objects for describing template code that can be generated from dwa.xml."""

import os
import sys
import dwa_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import codegen_shared


class EventInfo(codegen_shared.ModelTypeInfo):
  pass


class MetricInfo(codegen_shared.ModelTypeInfo):
  pass


class StudyInfo:
  """A class to hold codegen information about study type."""

  def __init__(self, json_obj: dict) -> None:
    self.raw_name = json_obj['name']
    self.name = codegen_shared._SanitizeName(json_obj['name'])
    self.hash = codegen_shared.HashFieldTrialName(json_obj['name'])


class Template(object):
  """Template for producing code from dwa.xml."""

  def __init__(self, basename: str, file_template: str, event_template: str,
               metric_template: str, study_template: str) -> None:
    self.basename = basename
    self.file_template = file_template
    self.event_template = event_template
    self.metric_template = metric_template
    self.study_template = study_template

  def _StampMetricCode(self, file_info: codegen_shared.FileInfo,
                       event_info: EventInfo, metric: dict) -> str:
    return self.metric_template.format(file=file_info,
                                       event=event_info,
                                       metric=MetricInfo(metric))

  def _StampStudyCode(self, file_info: codegen_shared.FileInfo,
                      event_info: EventInfo, study: dict) -> str:
    return self.study_template.format(file=file_info,
                                      event=event_info,
                                      study=StudyInfo(study))

  def _StampEventCode(self, file_info: codegen_shared.FileInfo,
                      event: dict) -> str:
    event_info = EventInfo(event)
    metric_code = "".join(
        self._StampMetricCode(file_info, event_info, metric)
        for metric in event[dwa_model._METRIC_TYPE.tag])
    study_code = "".join(
        self._StampStudyCode(file_info, event_info, study)
        for study in event[dwa_model._STUDY_TYPE.tag])
    return self.event_template.format(file=file_info,
                                      event=event_info,
                                      metric_code=metric_code,
                                      study_code=study_code)

  def _StampFileCode(self, relpath: str, data: dict) -> str:
    file_info = codegen_shared.FileInfo(relpath, self.basename)
    event_code = "".join(
        self._StampEventCode(file_info, event)
        for event in data[dwa_model._EVENT_TYPE.tag])
    return self.file_template.format(file=file_info, event_code=event_code)

  def WriteFile(self, outdir: str, relpath: str, data: dict) -> None:
    """Generates code and writes it to a file.

    Args:
      relpath: The path to the file in the source tree.
      rootdir: The root of the path the file should be written to.
      data: The parsed dwa.xml data.
    """
    output = open(os.path.join(outdir, self.basename), 'w')
    output.write(self._StampFileCode(relpath, data))
    output.close()

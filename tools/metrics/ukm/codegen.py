# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Objects for describing template code that can be generated from ukm.xml."""

import os
import sys
import ukm_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import codegen_shared


class EventInfo(codegen_shared.ModelTypeInfo):
  pass


class MetricInfo(codegen_shared.ModelTypeInfo):
  pass


class Template(object):
  """Template for producing code from ukm.xml."""

  def __init__(self, basename, file_template, event_template, metric_template):
    self.basename = basename
    self.file_template = file_template
    self.event_template = event_template
    self.metric_template = metric_template

  def _StampMetricCode(self, file_info, event_info, metric):
    return self.metric_template.format(
        file=file_info,
        event=event_info,
        metric=MetricInfo(metric))

  def _StampEventCode(self, file_info, event):
    event_info = EventInfo(event)
    metric_code = "".join(
        self._StampMetricCode(file_info, event_info, metric)
        for metric in event[ukm_model._METRIC_TYPE.tag])
    return self.event_template.format(
        file=file_info,
        event=event_info,
        metric_code=metric_code)

  def _StampFileCode(self, relpath, data):
    file_info = codegen_shared.FileInfo(relpath, self.basename)
    event_code = "".join(
        self._StampEventCode(file_info, event)
        for event in data[ukm_model._EVENT_TYPE.tag])
    return self.file_template.format(
        file=file_info,
        event_code=event_code)

  def WriteFile(self, outdir, relpath, data):
    """Generates code and writes it to a file.

    Args:
      relpath: The path to the file in the source tree.
      rootdir: The root of the path the file should be written to.
      data: The parsed ukm.xml data.
    """
    output = open(os.path.join(outdir, self.basename), 'w')
    output.write(self._StampFileCode(relpath, data))
    output.close()

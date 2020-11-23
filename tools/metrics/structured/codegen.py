# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Objects for describing template code to be generated from structured.xml."""

import hashlib
import os
import re
import struct
from model import _EVENT_TYPE, _EVENTS_TYPE
from model import _PROJECT_TYPE, _PROJECTS_TYPE
from model import _METRIC_TYPE


def sanitize_name(name):
  s = re.sub('[^0-9a-zA-Z_]', '_', name)
  return s


def HashName(name):
  # This must match the hash function in base/metrics/metric_hashes.cc
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(name).digest()[:8])[0]


class FileInfo(object):
  def __init__(self, relpath, basename):
    self.dir_path = relpath
    self.guard_path = sanitize_name(os.path.join(relpath, basename)).upper()


class EventInfo(object):
  def __init__(self, event_obj, project_obj):
    self.raw_name = event_obj['name']
    self.name = sanitize_name(event_obj['name'])
    self.name_hash = HashName(event_obj['name'])

    project_name = sanitize_name(project_obj['name'])
    self.project_name_hash = HashName(project_name)

    id_type = project_obj['id']['text']
    if id_type == 'uma':
      self.project_id_type = 'kUmaId'
    elif id_type == 'per-project':
      self.project_id_type = 'kProjectId'
    elif id_type == 'none':
      self.project_id_type = 'kUnidentified'
    else:
      raise Exception(
          "Structured metrics event '{}' has invalid id field '{}'".format(
              self.name, id_type))


class MetricInfo(object):
  def __init__(self, json_obj):
    self.raw_name = json_obj['name']
    self.name = sanitize_name(json_obj['name'])
    self.hash = HashName(json_obj['name'])
    if json_obj['kind'] == 'hashed-string':
      self.type = 'std::string&'
      self.setter = 'AddStringMetric'
    elif json_obj['kind'] == 'int':
      self.type = 'int'
      self.setter = 'AddIntMetric'
    else:
      raise Exception("Unexpected metric kind: " + json_obj['kind'])

class Template(object):
  """Template for producing code from structured.xml."""

  def __init__(self, basename, file_template, event_template, metric_template):
    self.basename = basename
    self.file_template = file_template
    self.event_template = event_template
    self.metric_template = metric_template

  def _StampMetricCode(self, file_info, event_info, metric):
    """Stamp a metric by creating name hash constant based on the metric name,
    and a setter method."""
    return self.metric_template.format(
        file=file_info,
        event=event_info,
        metric=MetricInfo(metric))

  def _StampEventCode(self, file_info, event, project):
    """Stamp an event class by creating a skeleton of the class based on the
    event name, and then stamping code for each metric within it."""
    event_info = EventInfo(event, project)
    metric_code = ''.join(
        self._StampMetricCode(file_info, event_info, metric)
        for metric in event[_METRIC_TYPE.tag])
    return self.event_template.format(
        file=file_info,
        event=event_info,
        metric_code=metric_code)

  def _StampFileCode(self, relpath, data):
    """Stamp a file by creating a class for each event, and a list of all event
    name hashes."""
    file_info = FileInfo(relpath, self.basename)
    event_code = []

    project_name_hashes = set()
    defined_projects = {
        project['name']: project
        for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag]
    }
    for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag]:
      defined_project = defined_projects.get(event.get('project'))
      event_code.append(self._StampEventCode(file_info, event, defined_project))
      project_name_hashes.add(
          defined_project['name'] if defined_project else event['name'])

    event_code = ''.join(event_code)

    project_name_hashes = [
        'UINT64_C(%s)' % HashName(name)
        for name in sorted(list(project_name_hashes))
    ]
    project_name_hashes = '{' + ', '.join(project_name_hashes) + '}'

    return self.file_template.format(
        file=file_info,
        event_code=event_code,
        project_name_hashes=project_name_hashes)

  def WriteFile(self, outdir, relpath, data):
    """Generates code and writes it to a file.

    Args:
      relpath: The path to the file in the source tree.
      rootdir: The root of the path the file should be written to.
      data: The parsed structured.xml data.
    """
    output = open(os.path.join(outdir, self.basename), 'w')
    output.write(self._StampFileCode(relpath, data))
    output.close()

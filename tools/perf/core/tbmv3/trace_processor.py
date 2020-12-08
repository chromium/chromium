# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess
import threading

from collections import namedtuple

from core.perfetto_binary_roller import binary_deps_manager
from py_utils import tempfile_ext
from tracing.value import histogram_set


TP_BINARY_NAME = 'trace_processor_shell'
EXPORT_JSON_QUERY_TEMPLATE = 'select export_json(%s)\n'
METRICS_PATH = os.path.realpath(os.path.join(os.path.dirname(__file__),
                                             'metrics'))
POWER_PROFILE_SQL = 'power_profile.sql'

MetricFiles = namedtuple('MetricFiles', ('sql', 'proto', 'internal_metric'))


class InvalidTraceProcessorOutput(Exception):
  pass

# This will be set to a path once trace processor has been fetched
# to avoid downloading several times during one Results Processor run.
_fetched_trace_processor = None
_fetch_lock = threading.Lock()


def _SqlString(s):
  """Produce a valid SQL string constant."""
  return "'%s'" % s.replace("'", "''")


def _EnsureTraceProcessor(trace_processor_path):
  global _fetched_trace_processor

  if trace_processor_path is None:
    with _fetch_lock:
      if not _fetched_trace_processor:
        _fetched_trace_processor = binary_deps_manager.FetchHostBinary(
            TP_BINARY_NAME)
        logging.info('Trace processor binary downloaded to %s',
                     _fetched_trace_processor)
    trace_processor_path = _fetched_trace_processor

  if not os.path.isfile(trace_processor_path):
    raise RuntimeError("Can't find trace processor executable at %s" %
                       trace_processor_path)
  return trace_processor_path


def _RunTraceProcessor(*args):
  """Run trace processor shell with given command line arguments."""
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode == 0:
    return stdout
  else:
    raise RuntimeError(
        'Running trace processor failed. Command line:\n%s\nStderr:\n%s\n' %
        (' '.join(args), stderr))


def _CreateMetricFiles(metric_name):
  # Currently assuming all metric files live in tbmv3/metrics directory unless
  # the metrics are compiled into trace processor. We will revise this decision
  # later.
  sql_file = os.path.join(METRICS_PATH, metric_name + '.sql')
  proto_file = os.path.join(METRICS_PATH, metric_name + '.proto')
  internal_metric = False
  if not (os.path.isfile(sql_file) and os.path.isfile(proto_file)):
    # Metric files not found - metric may be compiled into trace processor.
    internal_metric = True
  return MetricFiles(sql=sql_file,
                     proto=proto_file,
                     internal_metric=internal_metric)


def _ScopedHistogramName(metric_name, histogram_name):
  """Returns scoped histogram name by preprending metric name.

  This is useful for avoiding histogram name collision. The '_metric' suffix of
  the metric name is dropped from scoped name. Example:
  _ScopedHistogramName("console_error_metric", "js_errors")
  => "console_error::js_errors"
  """
  metric_suffix = '_metric'
  suffix_length = len(metric_suffix)
  # TODO(crbug.com/1012687): Decide on whether metrics should always have
  # '_metric' suffix.
  if metric_name[-suffix_length:] == metric_suffix:
    scope = metric_name[:-suffix_length]
  else:
    scope = metric_name
  return '::'.join([scope, histogram_name])


class ProtoFieldInfo(object):
  def __init__(self, name, parent, repeated, field_options):
    self.name = name
    self.parent = parent
    self.repeated = repeated
    self.field_options = field_options

  @property
  def path_from_root(self):
    if self.parent is None:
      return [self]
    else:
      return self.parent.path_from_root + [self]

  def __repr__(self):
    return 'ProtoFieldInfo("%s", repeated=%s)' %(self.name, self.repeated)


def _LeafFieldAnnotations(annotations, parent=None):
  """Yields leaf fields in the annotations tree, yielding a proto field info
  each time. Given the following annotation:
  __annotations: {
    a: {
      __field_options: { },
      b: { __field_options: { unit: "count" }, __repeated: True },
      c: { __field_options: { unit: "ms" } }
    }
  }
  It yields:
  ProtoFieldInfo(name="b", parent=FieldInfoForA,
                 repeated=True, field_options={unit: "count"})
  ProtoFieldInfo(name="c", parent=FieldInfoForA,
                 repeated=False, field_options={unit: "ms"})

  """
  for (name, field_value) in annotations.items():
    if name[:2] == "__":
      continue  # internal fields.
    current_field = ProtoFieldInfo(
        name=name,
        parent=parent,
        repeated=field_value.get('__repeated', False),
        field_options=field_value.get('__field_options', {}))
    has_no_descendants = True
    for descendant in _LeafFieldAnnotations(field_value, current_field):
      has_no_descendants = False
      yield descendant
    if has_no_descendants:
      yield current_field


def _PluckField(json_dict, field_path):
  """Returns the values of fields matching field_path from json dict.

  Field path is a sequence of ProtoFieldInfo starting from the root dict. Arrays
  are flattened along the way. For exampe, consider the following json dict:
  {
    a: {
      b: [ { c: 24 }, { c: 25 } ],
      d: 42,
    }
  }
  Field path (a, d) returns [42]. Field_path (a, b, c) returns [24, 25].
  """
  if len(field_path) == 0:
    return [json_dict]
  path_head = field_path[0]
  path_tail = field_path[1:]

  if path_head.repeated:
    field_values = json_dict[path_head.name]
    if not isinstance(field_values, list):
      raise InvalidTraceProcessorOutput(
          "Field marked as repeated but json value is not list")
    output = []
    for field_value in field_values:
      output.extend(_PluckField(field_value, path_tail))
    return output
  else:
    field_value = json_dict[path_head.name]
    if isinstance(field_value, list):
      raise InvalidTraceProcessorOutput(
          "Field not marked as repeated but json value is list")
    return _PluckField(field_value, path_tail)


def RunMetrics(trace_processor_path, trace_file, metric_names,
               fetch_power_profile=False):
  """Run TBMv3 metrics using trace processor.

  Args:
    trace_processor_path: path to the trace_processor executable.
    trace_file: path to the trace file.
    metric_names: a list of metric names (the corresponding files must exist in
        tbmv3/metrics directory).

  Returns:
    A HistogramSet with metric results.
  """
  trace_processor_path = _EnsureTraceProcessor(trace_processor_path)
  metric_name_args = []
  for metric_name in metric_names:
    metric_files = _CreateMetricFiles(metric_name)
    if metric_files.internal_metric:
      metric_name_args.append(metric_name)
    else:
      metric_name_args.append(metric_files.sql)
  command_args = [
      trace_processor_path,
      '--run-metrics', ','.join(metric_name_args),
      '--metrics-output', 'json',
      trace_file,
  ]
  if fetch_power_profile:
    power_profile_sql = binary_deps_manager.FetchDataFile(POWER_PROFILE_SQL)
    command_args[1:1] = ['--pre-metrics', power_profile_sql]

  output = _RunTraceProcessor(*command_args)
  measurements = json.loads(output)

  histograms = histogram_set.HistogramSet()
  root_annotations = measurements.get('__annotations', {})
  for metric_name in metric_names:
    full_metric_name = 'perfetto.protos.' + metric_name
    annotations = root_annotations.get(full_metric_name, None)
    metric_proto = measurements.get(full_metric_name, None)
    if metric_proto is None:
      logging.warn("Metric not found in the output: %s", metric_name)
      continue
    elif annotations is None:
      logging.info("Skipping metric %s because it has no field with unit.",
                   metric_name)
      continue

    for field in _LeafFieldAnnotations(annotations):
      unit = field.field_options.get('unit', None)
      if unit is None:
        logging.debug('Skipping field %s to histograms because it has no unit',
            field.name)
        continue
      histogram_name = ':'.join([field.name for field in field.path_from_root])
      samples = _PluckField(metric_proto, field.path_from_root)
      scoped_histogram_name = _ScopedHistogramName(metric_name, histogram_name)
      histograms.CreateHistogram(scoped_histogram_name, unit, samples)

  return histograms


def RunMetric(trace_processor_path, trace_file, metric_name,
              fetch_power_profile=False):
  return RunMetrics(trace_processor_path, trace_file, [metric_name],
                    fetch_power_profile)


def ConvertProtoTraceToJson(trace_processor_path, proto_file, json_path):
  """Convert proto trace to json using trace processor.

  Args:
    trace_processor_path: path to the trace_processor executable.
    proto_file: path to the proto trace file.
    json_path: path to the output file.

  Returns:
    Output path.
  """
  trace_processor_path = _EnsureTraceProcessor(trace_processor_path)
  with tempfile_ext.NamedTemporaryFile() as query_file:
    query_file.write(EXPORT_JSON_QUERY_TEMPLATE % _SqlString(json_path))
    query_file.close()
    _RunTraceProcessor(
        trace_processor_path,
        '-q', query_file.name,
        proto_file,
    )

  return json_path

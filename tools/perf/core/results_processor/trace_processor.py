# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import logging
import os
import shutil
import subprocess

from core import path_util
from py_utils import tempfile_ext


DEFAULT_TP_PATH = os.path.join(
    path_util.GetChromiumSrcDir(), 'out', 'Debug', 'trace_processor_shell')
EXPORT_JSON_QUERY_TEMPLATE = 'select export_json(%s)\n'


def _SqlString(s):
  """Produce a valid SQL string constant."""
  return "'%s'" % s.replace("'", "''")


def ConvertProtoTracesToJson(trace_processor_path, proto_files, json_path):
  if not os.path.isfile(trace_processor_path):
    raise RuntimeError("Can't find trace processor executable at %s" %
                       trace_processor_path)

  with tempfile_ext.NamedTemporaryFile() as concatenated_trace:
    for trace_file in proto_files:
      if trace_file.endswith('.pb.gz'):
        with gzip.open(trace_file, 'rb') as f:
          shutil.copyfileobj(f, concatenated_trace)
      else:
        with open(trace_file, 'rb') as f:
          shutil.copyfileobj(f, concatenated_trace)
    concatenated_trace.close()

    with tempfile_ext.NamedTemporaryFile() as query_file:
      query_file.write(EXPORT_JSON_QUERY_TEMPLATE % _SqlString(json_path))
      query_file.close()
      subprocess.check_call([
          trace_processor_path,
          concatenated_trace.name,
          '-q', query_file.name,
      ])

  logging.info('Converted json trace written to %s', json_path)

  return json_path

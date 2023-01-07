# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for CSV format."""

import collections
import csv
import json
import os

from py_utils import tempfile_ext
from tracing.value import histograms_to_csv


OUTPUT_FILENAME = 'results.csv'


def _ReadCsv(input_stream):
  dicts = []
  header = None
  for row in csv.reader(input_stream):
    if header is None:
      header = row
    elif row:
      dicts.append(collections.OrderedDict(zip(header, row)))
  return dicts


def _WriteCsv(dicts, output_stream):
  header = []
  for d in dicts:
    for k in d:
      if k not in header:
        header.append(k)
  rows = [header]
  for d in dicts:
    rows.append([d.get(k, '') for k in header])
  csv.writer(output_stream).writerows(rows)


def ProcessHistogramDicts(histogram_dicts, options):
  """Convert histogram dicts to CSV and write output in output_dir."""
  with tempfile_ext.NamedTemporaryFile(mode='w') as hist_file:
    json.dump(histogram_dicts, hist_file)
    hist_file.close()
    vinn_result = histograms_to_csv.HistogramsToCsv(hist_file.name)
    csv_dicts = _ReadCsv(vinn_result.stdout.decode('utf-8').splitlines())

  output_file = os.path.join(options.output_dir, OUTPUT_FILENAME)
  if not options.reset_results and os.path.isfile(output_file):
    with open(output_file) as input_stream:
      csv_dicts += _ReadCsv(input_stream)

  with open(output_file, 'w') as output_stream:
    _WriteCsv(csv_dicts, output_stream)

  return output_file

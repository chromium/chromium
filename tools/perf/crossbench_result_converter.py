#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Converts crossbench result into histogram format.

See example inputs in testdata/crossbench_output folder.
"""

import argparse
import csv
import json
import pathlib
import sys
from typing import List, Optional

tracing_dir = (pathlib.Path(__file__).absolute().parents[2] /
               'third_party/catapult/tracing')
sys.path.append(str(tracing_dir))
from tracing.value import histogram, histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


def _get_crossbench_json_paths(out_dir: pathlib.Path) -> List[pathlib.Path]:
  """Given a crossbench output directory, find the result json file.

  Args:
    out_dir: Crossbench output directory. This should be the value passed
        as --out-dir to crossbench.

  Returns:
    A list of paths to the result json files created by crossbench probes.
  """

  if not out_dir.exists():
    raise FileNotFoundError(
        f'Crossbench output directory does not exist: {out_dir}')

  cb_results_json_path = out_dir / 'cb.results.json'
  if not cb_results_json_path.exists():
    raise FileNotFoundError(
        f'Missing crossbench results file: {cb_results_json_path}')

  debug_info = ''
  with cb_results_json_path.open() as f:
    results_info = json.load(f)
    debug_info += f'results_info={results_info}\n'

  browsers = results_info.get('browsers', {})
  if len(browsers) != 1:
    raise ValueError(
        f'Expected to have one "browsers" in {cb_results_json_path}, '
        f'debug_info={debug_info}')
  browser_info = list(browsers.values())[0]
  debug_info += f'browser_info={browser_info}\n'

  probe_json_paths = []
  try:
    for probe, probe_data in browser_info.get('probes', {}).items():
      if probe == 'trace_processor':
        probe_data = results_info.get('probes', {}).get('trace_processor')
      if probe.startswith('cb.') or not probe_data:
        continue
      candidates = probe_data.get('json', [])
      if len(candidates) > 1:
        raise ValueError(f'Probe {probe} generated multiple json files, '
                         f'debug_info={debug_info}')
      if len(candidates) == 1:
        probe_json_paths.append(pathlib.Path(candidates[0]))
  except AttributeError as e:
    raise AttributeError(f'debug_info={debug_info}') from e

  if not probe_json_paths:
    raise ValueError(f'No output json file found in {cb_results_json_path}, '
                     f'debug_info={debug_info}')

  return probe_json_paths


def convert(crossbench_out_dir: pathlib.Path,
            out_filename: pathlib.Path,
            benchmark: Optional[str] = None,
            story: Optional[str] = None,
            results_label: Optional[str] = None) -> None:
  """Do the conversion of crossbench output into histogram format.

  Args: See the help strings passed to argparse.ArgumentParser.
  """

  if benchmark and benchmark.startswith('loadline'):
    _loadline(crossbench_out_dir, out_filename, benchmark, results_label)
    return

  crossbench_json_filenames = _get_crossbench_json_paths(crossbench_out_dir)
  crossbench_result = {}
  for filename in crossbench_json_filenames:
    with filename.open() as f:
      crossbench_result.update(json.load(f))

  results = histogram_set.HistogramSet()
  for key, value in crossbench_result.items():
    metric = None
    key_parts = key.split('/')
    if len(key_parts) == 1:
      if key.startswith('Iteration') or key == 'Geomean':
        continue
      metric = key
      if key.lower() == 'score':
        unit = 'unitless_biggerIsBetter'
      else:
        unit = 'ms_smallerIsBetter'
    else:
      if len(key_parts) == 2 and key_parts[1] == 'total':
        metric = key_parts[0]
        unit = 'ms_smallerIsBetter'
      elif len(key_parts) == 2 and key_parts[1] == 'score':
        metric = key_parts[0]
        unit = 'unitless_biggerIsBetter'

    if metric:
      data_point = histogram.Histogram.Create(metric, unit, value['values'])
      results.AddHistogram(data_point)

  if benchmark:
    results.AddSharedDiagnosticToAllHistograms(
        reserved_infos.BENCHMARKS.name, generic_set.GenericSet([benchmark]))
  if story:
    results.AddSharedDiagnosticToAllHistograms(
        reserved_infos.STORIES.name, generic_set.GenericSet([story]))
  if results_label:
    results.AddSharedDiagnosticToAllHistograms(
        reserved_infos.LABELS.name, generic_set.GenericSet([results_label]))

  with out_filename.open('w') as f:
    json.dump(results.AsDicts(), f)


def _loadline(crossbench_out_dir: pathlib.Path,
              out_filename: pathlib.Path,
              benchmark: Optional[str] = None,
              results_label: Optional[str] = None) -> None:
  """Converts `loadline-*` benchmarks."""

  crossbench_json_filename = crossbench_out_dir / 'cb.results.json'
  if not crossbench_json_filename.exists():
    raise FileNotFoundError(
        f'Missing crossbench results file: {crossbench_json_filename}')

  with crossbench_json_filename.open() as f:
    crossbench_result = json.load(f)

  loadline_csv = pathlib.Path(
      crossbench_result["probes"]["loadline_probe"]["csv"][0])
  if not loadline_csv.exists():
    raise FileNotFoundError(
        f'Missing loadline CSV results file: {loadline_csv}')
  with loadline_csv.open() as f:
    loadline_result = next(csv.DictReader(f))

  results = histogram_set.HistogramSet()
  for key, value in loadline_result.items():
    data_point = None
    if key == 'browser':
      results.AddSharedDiagnosticToAllHistograms(
          key, generic_set.GenericSet([value]))
    else:
      data_point = histogram.Histogram.Create(key, 'unitless_biggerIsBetter',
                                              float(value))
    if data_point:
      results.AddHistogram(data_point)

  if benchmark:
    results.AddSharedDiagnosticToAllHistograms(
        reserved_infos.BENCHMARKS.name, generic_set.GenericSet([benchmark]))
  if results_label:
    results.AddSharedDiagnosticToAllHistograms(
        reserved_infos.LABELS.name, generic_set.GenericSet([results_label]))

  with out_filename.open('w') as f:
    json.dump(results.AsDicts(), f)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('crossbench_out_dir',
                      type=pathlib.Path,
                      help='value of --out-dir passed to crossbench')
  parser.add_argument('out_filename',
                      type=pathlib.Path,
                      help='name of output histogram file to generate')
  parser.add_argument('--benchmark', help='name of the benchmark')
  parser.add_argument('--story', help='name of the story')

  args = parser.parse_args()

  convert(args.crossbench_out_dir,
          args.out_filename,
          benchmark=args.benchmark,
          story=args.story)


if __name__ == '__main__':
  sys.exit(main())

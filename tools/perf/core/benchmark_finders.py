# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import imp
import inspect
import os
import sys

from core import path_util
from core import perf_benchmark

from telemetry import benchmark as benchmark_module

from py_utils import discover

def GetClassFilePath(clazz):
  """ Return the absolute file path to |clazz|. """
  assert inspect.isclass(clazz)
  path = os.path.abspath(inspect.getfile(clazz))
  if path.endswith('.pyc'):
    return path[:-1]
  return path


def GetBenchmarkNamesForFile(top_level_dir, benchmark_file_dir):
  """  Return the list of all benchmark names of benchmarks defined in
    |benchmark_file_dir|.
  """
  original_sys_path = sys.path[:]
  top_level_dir = os.path.abspath(top_level_dir)
  original_sys_path = sys.path[:]
  if top_level_dir not in sys.path:
    sys.path.append(top_level_dir)
  try:
    module = imp.load_source('_tmp_module_name_', benchmark_file_dir)
    benchmark_names = []
    for _, obj in inspect.getmembers(module):
      if (inspect.isclass(obj) and issubclass(obj, perf_benchmark.PerfBenchmark)
          and GetClassFilePath(obj) == benchmark_file_dir):
        benchmark_names.append(obj.Name())
    return sorted(benchmark_names)
  finally:
    sys.path = original_sys_path


def GetOfficialBenchmarks():
  """Returns the list of all benchmarks to be run on perf waterfall.
  The benchmarks are sorted by order of their names.
  """
  benchmarks = list(
      discover.DiscoverClasses(start_dir=path_util.GetOfficialBenchmarksDir(),
                               top_level_dir=path_util.GetPerfDir(),
                               base_class=benchmark_module.Benchmark,
                               index_by_class_name=True).values())
  benchmarks.sort(key=lambda b: b.Name())
  return benchmarks


def GetContribBenchmarks():
  """Returns the list of all contrib benchmarks.
  The benchmarks are sorted by order of their names.
  """
  _BENCHMARKS_TO_SKIP = {
      # This benchmark is intended to be a convenience base class for writing
      # other benchmarks, and not a standalone benchmark.
      'perf_benchmark_with_profiling.PerfBenchmarkWithProfiling',
      'perf_benchmark_with_profiling_unittest.PerfBenchmarkForTesting',
      # This benchmark is a base class for power measurements.
      'power_perf_benchmark_base.PowerPerfBenchmarkBase',
  }

  benchmarks = list(
      discover.DiscoverClasses(start_dir=path_util.GetContribDir(),
                               top_level_dir=path_util.GetPerfDir(),
                               base_class=benchmark_module.Benchmark,
                               index_by_class_name=True).values())
  benchmarks = [
      benchmark for benchmark in benchmarks
      if benchmark.Name() not in _BENCHMARKS_TO_SKIP
  ]
  benchmarks.sort(key=lambda b: b.Name())
  return benchmarks


def GetAllBenchmarks():
  """Returns all benchmarks in tools/perf directory.
  The benchmarks are sorted by order of their names.
  """
  waterfall_benchmarks = GetOfficialBenchmarks()
  contrib_benchmarks = GetContribBenchmarks()
  benchmarks = waterfall_benchmarks + contrib_benchmarks
  benchmarks.sort(key=lambda b: b.Name())
  return benchmarks


def GetBenchmarksInSubDirectory(directory):
  return list(
      discover.DiscoverClasses(start_dir=directory,
                               top_level_dir=path_util.GetPerfDir(),
                               base_class=benchmark_module.Benchmark,
                               index_by_class_name=True).values())

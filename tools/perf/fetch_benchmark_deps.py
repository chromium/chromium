#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module fetches and prints the dependencies given a benchmark."""

import argparse
import json
import os
import sys
import logging
from six.moves import input  # pylint: disable=redefined-builtin

from chrome_telemetry_build import chromium_config
from core import benchmark_finders
from core import path_util
from page_sets import speedometer3_pages
from py_utils import cloud_storage

from telemetry.core import optparse_argparse_migration as oam
from telemetry.core import platform as platform_module
from telemetry.internal.util import binary_manager

def _FetchDependenciesIfNeeded(story_set):
  """ Download files needed by a user story set. """
  # Download files in serving_dirs.
  serving_dirs = story_set.serving_dirs
  for directory in serving_dirs:
    cloud_storage.GetFilesInDirectoryIfChanged(directory, story_set.bucket)

  if not story_set.wpr_archive_info:
    return

  # Download WPR files.
  story_names = [s.name for s in story_set if not s.is_local]
  story_set.wpr_archive_info.DownloadArchivesIfNeeded(story_names=story_names)


def _EnumerateDependencies(story_set):
  """Enumerates paths of files needed by a user story set."""
  deps = set()
  # Enumerate WPRs
  for story in story_set:
    deps.add(story_set.WprFilePathForStory(story))

  # Enumerate files in serving_dirs
  for directory in story_set.serving_dirs:
    if not os.path.isdir(directory):
      raise ValueError('Must provide a valid directory.')
    # Don't allow the root directory to be a serving_dir.
    if directory == os.path.abspath(os.sep):
      raise ValueError('Trying to serve root directory from HTTP server.')
    for dirpath, _, filenames in os.walk(directory):
      for filename in filenames:
        path_name, extension = os.path.splitext(
            os.path.join(dirpath, filename))
        if extension == '.sha1':
          deps.add(path_name)

  # Return relative paths.
  prefix_len = len(os.path.realpath(path_util.GetChromiumSrcDir())) + 1
  return [dep[prefix_len:] for dep in deps if dep]


def _FetchDepsForBenchmark(benchmark):
  # Create a dummy options object which hold default values that are expected
  # by Benchmark.CreateStorySet(options) method.
  parser = oam.CreateFromOptparseInputs()
  benchmark.AddBenchmarkCommandLineArgs(parser)
  options, _ = parser.parse_args([])
  story_set = benchmark().CreateStorySet(options)

  # Download files according to specified benchmark.
  _FetchDependenciesIfNeeded(story_set)

  # Log files downloaded.
  logging.info('Fetch dependencies for benchmark %s' % benchmark.Name())
  deps = _EnumerateDependencies(story_set)
  for dep in deps:
    logging.info("Dependency: " + dep)
  return deps


def FetchDepsForCrossbench():
  # TODO: Fetch all crossbench archive files when they are available.
  story_set = speedometer3_pages.Speedometer30CrossbenchStory()
  story_names = [s.name for s in story_set]
  story_set.wpr_archive_info.DownloadArchivesIfNeeded(story_names=story_names)
  platform = platform_module.GetHostPlatform()
  binary_manager.InitDependencyManager(None)
  binary_manager.FetchBinaryDependencies(
      platform,
      client_configs=[],
      fetch_reference_chrome_binary=False,
      dependency_filter=['wpr_go', 'httparchive_go'])


def main(args):
  parser = argparse.ArgumentParser(
         description='Fetch the dependencies of perf benchmark(s).')
  parser.add_argument('benchmark_name', type=str, nargs='?')
  parser.add_argument('--force', '-f',
                      help=('Force fetching all the benchmarks when '
                            'benchmark_name is not specified'),
                      action='store_true', default=False)
  parser.add_argument('--platform', '-p',
                      help=('Only fetch benchmarks for the specified platform '
                            '(win, linux, mac, android)'),
                      default=None)
  # Flag --output-deps: output the dependencies to a json file, CrOS autotest
  # telemetry_runner parses the output to upload the dependencies to the DUT.
  # Example output, fetch_benchmark_deps.py --output-deps=deps octane:
  # {'octane': ['tools/perf/page_sets/data/octane_002.wprgo']}
  parser.add_argument('--output-deps',
                      help=('Output dependencies to a json file'))
  parser.add_argument(
        '-v', '--verbose', action='count', dest='verbosity', default=0,
        help='Increase verbosity level (repeat as needed)')

  options = parser.parse_args(args)

  if options.verbosity >= 2:
    logging.getLogger().setLevel(logging.DEBUG)
  elif options.verbosity:
    logging.getLogger().setLevel(logging.INFO)
  else:
    logging.getLogger().setLevel(logging.WARNING)

  deps = {}
  if options.benchmark_name:
    perf_dir = path_util.GetPerfDir()
    benchmark_dirs=[os.path.join(perf_dir, 'benchmarks'),
                    os.path.join(perf_dir, 'contrib')]
    config = chromium_config.ChromiumConfig(
        top_level_dir=path_util.GetPerfDir(), benchmark_dirs=benchmark_dirs)
    benchmark = config.GetBenchmarkByName(options.benchmark_name)
    if not benchmark:
      raise ValueError('No such benchmark: %s' % options.benchmark_name)
    deps[benchmark.Name()] = _FetchDepsForBenchmark(benchmark)
  else:
    if not options.force:
      input('No benchmark name is specified. Fetching all benchmark deps. '
            'Press enter to continue...')
    for b in benchmark_finders.GetOfficialBenchmarks():
      supported_platforms = b.GetSupportedPlatformNames(b.SUPPORTED_PLATFORMS)
      if(not options.platform or
         options.platform in supported_platforms or
         'all' in supported_platforms):
        deps[b.Name()] = _FetchDepsForBenchmark(b)

  FetchDepsForCrossbench()

  if options.output_deps:
    with open(options.output_deps, 'w') as outfile:
      json.dump(deps, outfile)


if __name__ == '__main__':
  main(sys.argv[1:])

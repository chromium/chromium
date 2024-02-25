# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from telemetry import benchmark as b_module
from telemetry.core import optparse_argparse_migration as oam
from telemetry.internal.browser import browser_options


StoryInfo = collections.namedtuple('StoryInfo', ['name', 'description', 'tags'])


def GetBenchmarkStorySet(benchmark, exhaustive=False):
  if not isinstance(benchmark, b_module.Benchmark):
    raise ValueError(
      '|benchmark| must be an instace of telemetry.benchmark.Benchmark class. '
      'Instead found object of type: %s' % type(benchmark))
  options = browser_options.BrowserFinderOptions()
  # Add default values for any extra commandline options
  # provided by the benchmark.
  parser = oam.CreateFromOptparseInputs()
  before, _ = parser.parse_args([])
  benchmark.AddBenchmarkCommandLineArgs(parser)
  after, _ = parser.parse_args([])
  for extra_option in dir(after):
    if extra_option not in dir(before):
      setattr(options, extra_option, getattr(after, extra_option))
  if exhaustive:
    setattr(options, 'story_set_should_be_exhaustive_for_test', True)
  return benchmark.CreateStorySet(options)


def GetBenchmarkStoryInfo(benchmark):
  """Return a list with StoryInfo objects for each story in a benchmark."""
  stories = [
      StoryInfo(name=story.name, description=DescribeStory(story),
                tags=list(story.tags))
      for story in GetBenchmarkStorySet(benchmark)
  ]
  stories.sort(key=lambda s: s.name)
  return stories


def GetBenchmarkStoryNames(benchmark):
  """Return the list of all stories names in the benchmark.
  This guarantees the order of the stories in the list is exactly the same
  of the order of stories to be run by benchmark.
  """
  story_list = []
  for story in GetBenchmarkStorySet(benchmark):
    story_list.append(story.name)
  return story_list


def DescribeStory(story):
  """Get the docstring title out of a given story."""
  description = story.__doc__
  if description:
    return description.strip().splitlines()[0]
  return ''

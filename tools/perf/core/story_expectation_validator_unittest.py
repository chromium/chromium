# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from core import story_expectation_validator

from telemetry import benchmark
from telemetry import story

from typ import expectations_parser as typ_expectations_parser

class FakePage(object):
  def __init__(self, name):
    self._name = name

  @property
  def name(self):
    return self._name


class FakeStorySetOne(story.StorySet):
  def __init__(self): # pylint: disable=super-init-not-called
    self._stories = [
        FakePage('One'),
        FakePage('Two')
    ]

  @property
  def stories(self):
    return self._stories


class FakeBenchmark(benchmark.Benchmark):
  @classmethod
  def Name(cls):
    return 'b1'

  def CreateStorySet(self, options):
    return FakeStorySetOne()


class StoryExpectationValidatorTest(unittest.TestCase):
  def testValidateStoryInValidName(self):
    raw_expectations = ('# tags: [ Mac ]\n'
                        '# results: [ Skip ]\n'
                        'crbug.com/123 [ Mac ] b1/s1 [ Skip ]\n')
    test_expectations = typ_expectations_parser.TestExpectations()
    ret, _ = test_expectations.parse_tagged_list(raw_expectations)
    self.assertFalse(ret)
    benchmarks = [FakeBenchmark]
    with self.assertRaises(AssertionError):
      story_expectation_validator.validate_story_names(
          benchmarks, test_expectations)

  def testValidateStoryValidName(self):
    raw_expectations = ('# tags: [ Mac] \n'
                        '# results: [ Skip ]\n'
                        'crbug.com/123 [ Mac ] b1/One [ Skip ]\n')
    test_expectations = typ_expectations_parser.TestExpectations()
    ret, _ = test_expectations.parse_tagged_list(raw_expectations)
    self.assertFalse(ret)
    benchmarks = [FakeBenchmark]
    # If a name is invalid, an exception is thrown. If no exception is thrown
    # all story names are valid. That is why there is no assert here.
    story_expectation_validator.validate_story_names(
        benchmarks, test_expectations)

  def testValidateExpectationsComponentTags(self):
    raw_expectations = ('# tags: [ android mac ]\n'
                        '# tags: [ android-webview ]\n'
                        '# results: [ Skip ]\n'
                        'crbug.com/123 [ mac android-webview ]'
                        ' b1/s1 [ Skip ]\n')
    test_expectations = typ_expectations_parser.TestExpectations()
    ret, _ = test_expectations.parse_tagged_list(raw_expectations)
    self.assertFalse(ret)
    with self.assertRaises(AssertionError):
      story_expectation_validator.validate_expectations_component_tags(
          test_expectations)

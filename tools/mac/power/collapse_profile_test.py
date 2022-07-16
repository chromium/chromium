# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from collapse_profile import StackCollapser


class DTraceReadTest(unittest.TestCase):
  def testEmpty(self):
    """Tests that a directory with no valid stacks triggers a failure."""

    with self.assertRaises(SystemExit):
      collapser = StackCollapser('./samples.collapsed')
      collapser.read_dtrace_logs('./test_data/empty/')

  def testValidBlock(self):
    """Tests basic parsing of the DTrace format."""

    collapser = StackCollapser('./samples.collapsed')
    collapser.read_dtrace_logs('./test_data/valid/')
    self.assertEquals(collapser.samples, [{
        'frames': ['foo', 'bar', 'baz'],
        'weight': 12
    }])

  def testRepeatedFunction(self):
    """Tests accumulation of samples of the same function over many files."""

    collapser = StackCollapser('./samples.collapsed')
    collapser.read_dtrace_logs('./test_data/repeated/')
    self.assertEquals(collapser.samples, [{
        'frames': ['foo', 'bar', 'baz'],
        'weight': 24
    }])

  def testTrimFunctionOffset(self):
    """Tests removal of the function offset markers in the DTrace format."""

    collapser = StackCollapser('./samples.collapsed')
    collapser.read_dtrace_logs('./test_data/with_offset/')
    self.assertEquals(collapser.samples, [{
        'frames': ['foo', 'bar', 'baz'],
        'weight': 12
    }])


class StackCollapseTest(unittest.TestCase):
  def testDoublePostProcessFails(self):
    """Tests that calling post_process_samples() twice triggers a failure."""

    samples = [{'frames': ['foo', 'bar', 'baz'], 'weight': 24}]
    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)

    with self.assertRaises(SystemExit):
      stack_collapser.post_process_samples()
      stack_collapser.post_process_samples()

  def testNoStackShortening(self):
    """Tests that sampes with no uninteresting frames don't get modified."""

    samples = [{'frames': ['foo', 'bar', 'baz'], 'weight': 24}]
    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(samples, stack_collapser.samples)

  def testTokenClean(self):
    """Tests that tokens that need to be removed are cleaned and others
    are left untouched."""

    samples = [{
        'frames': ['Chromium Framework`foo', 'bar', 'baz'],
        'weight': 24
    }]
    cleaned_samples = [{'frames': ['foo', 'bar', 'baz'], 'weight': 24}]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(cleaned_samples, stack_collapser.samples)

  def testNoStackShortening(self):
    """Tests that sampes with no uninteresting frames don't get modified."""

    samples = [{'frames': ['foo', 'bar', 'baz'], 'weight': 24}]
    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(samples, stack_collapser.samples)

  def testCutoffPointPreserved(self):
    """Tests that shortening a stack is inclusive of the cutoff point."""

    samples = [{
        'frames':
        ['foo', 'bar', 'baz', 'base::MessagePumpNSRunLoop::DoRun', "biz"],
        'weight':
        24
    }]
    shortened_stack = [{
        'frames': ['base::MessagePumpNSRunLoop::DoRun', "biz"],
        'weight': 24
    }]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(shortened_stack, stack_collapser.samples)

  def testNoStackDisappearance(self):
    """Tests that a stack that finishes with an ignored frame isn't culled from
    the report. It represents overhead and should be kept"""

    samples = [{
        'frames': ['foo', 'bar', 'baz', 'base::MessagePumpNSRunLoop::DoRun'],
        'weight':
        24
    }]
    shortened_stack = [{
        'frames': ['base::MessagePumpNSRunLoop::DoRun'],
        'weight': 24
    }]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(shortened_stack, stack_collapser.samples)

  def testStackShortening(self):
    """Tests that stacks get shortened to drop frames of low interest."""

    samples = [{
        'frames': ['foo', 'bar', 'baz'],
        'weight': 24
    }, {
        'frames': ['foo', 'bar', 'baz', 'base::MessagePumpNSRunLoop::DoRun'],
        'weight':
        24
    }]

    # Stack will be shortened to remove anything before the uninteresting frame.
    shortened_samples = samples.copy()
    shortened_samples[1]["frames"] = shortened_samples[1]["frames"][2:]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(shortened_samples, stack_collapser.samples)

  def testSingleSyntheticMarker(self):
    """Tests that when a single frame of interest is found the appropriate
    synthetic frame is added to the bottom of the stack."""

    samples = [{'frames': ['foo', 'bar', 'baz'], 'weight': 24}]

    # Stack will be augmented with mojo marker
    augmented_stack = samples.copy()
    augmented_stack[0]["frames"] = ['viz::'] + augmented_stack[0]["frames"]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(augmented_stack, stack_collapser.samples)

  def testSynonymMarkers(self):
    """Tests that two markers that are synonyms trigger the addition of the same
    synthetic frame."""

    samples = [{'frames': ['foo', 'bar', '::network'], 'weight': 24}]
    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()

    samples_2 = [{'frames': ['foo', 'bar', '::net'], 'weight': 24}]
    stack_collapser_2 = StackCollapser('./samples.collapsed')
    stack_collapser_2.set_samples_for_testing(samples_2)
    stack_collapser_2.post_process_samples()

    # Same synthetic stack frame is added.
    self.assertEquals(stack_collapser.samples[0]["frames"][0],
                      stack_collapser_2.samples[0]["frames"][0])

  def testCompoundSyntheticMarker(self):
    """Tests that when more than one frame of interest is found the correct
    compound synthetic frame is added to the bottom of the stack."""

    samples = [{'frames': ['foo', 'bar', 'viz', '::network'], 'weight': 24}]

    # Stack will be augmented with mojo marker
    augmented_stack = samples.copy()
    augmented_stack[0]["frames"] = ['mojo::viz::'
                                    ] + augmented_stack[0]["frames"]

    stack_collapser = StackCollapser('./samples.collapsed')
    stack_collapser.set_samples_for_testing(samples)
    stack_collapser.post_process_samples()
    self.assertEquals(augmented_stack, stack_collapser.samples)

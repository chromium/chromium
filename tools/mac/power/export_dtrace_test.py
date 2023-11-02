# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from export_dtrace import DTraceParser


class DTraceReadTest(unittest.TestCase):
  def testEmpty(self):
    """Tests that a directory with no valid stacks triggers a failure."""

    with self.assertRaises(SystemExit):
      collapser = DTraceParser()
      collapser.ParseDir('./test_data/empty/')

  def testValidBlock(self):
    """Tests basic parsing of the DTrace format."""

    collapser = DTraceParser()
    collapser.ParseDir('./test_data/valid/')
    self.assertEquals(collapser.GetSamplesListForTesting(), [{
        'frames': [('fake_module', 'baz'), ('fake_module', 'bar'),
                   ('fake_module', 'foo')],
        'weight':
        12
    }])

  def testRepeatedFunction(self):
    """Tests accumulation of samples of the same function over many files."""

    collapser = DTraceParser()
    collapser.ParseDir('./test_data/repeated/')
    self.assertEquals(collapser.GetSamplesListForTesting(), [{
        'frames': [('fake_module', 'baz'), ('fake_module', 'bar'),
                   ('fake_module', 'foo')],
        'weight':
        24
    }])

  def testUnsymbolized(self):
    """Tests that absolute addresses are parsed as unsymbolized frames.
    """

    collapser = DTraceParser()
    collapser.ParseDir('./test_data/absolute_offset/')
    self.assertEquals(collapser.GetSamplesListForTesting(), [{
        'frames': [
            ('unsymbolized module', 'unsymbolized function'),
            ('unsymbolized module', 'unsymbolized function'),
            ('unsymbolized module', 'unsymbolized function'),
        ],
        'weight':
        12
    }])


class StackCollapseTest(unittest.TestCase):
  def testDoubleShortenStackSamplesFails(self):
    """Tests that calling post_process_samples() twice triggers a failure."""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz')],
        'weight':
        24
    }]
    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)

    with self.assertRaises(SystemExit):
      stack_collapser.ShortenStackSamples()
      stack_collapser.ShortenStackSamples()

  def testNoStackShortening(self):
    """Tests that sampes with no uninteresting frames don't get modified."""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz')],
        'weight':
        24
    }]
    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)
    stack_collapser.ShortenStackSamples()
    self.assertEquals(samples, stack_collapser.GetSamplesListForTesting())

  def testNoStackShortening(self):
    """Tests that sampes with no uninteresting frames don't get modified."""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz')],
        'weight':
        24
    }]
    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)
    stack_collapser.ShortenStackSamples()
    self.assertEquals(samples, stack_collapser.GetSamplesListForTesting())

  def testCutoffPointPreserved(self):
    """Tests that shortening a stack is inclusive of the cutoff point."""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz'),
                   ('chrome', 'base::MessagePumpNSRunLoop::DoRun'),
                   ('fake_module', 'biz')],
        'weight':
        24
    }]
    shortened_stack = [{
        'frames': [('chrome', 'base::MessagePumpNSRunLoop::DoRun'),
                   ('fake_module', 'biz')],
        'weight':
        24
    }]

    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)
    stack_collapser.ShortenStackSamples()
    self.assertEquals(shortened_stack,
                      stack_collapser.GetSamplesListForTesting())

  def testNoStackDisappearance(self):
    """Tests that a stack that finishes with an ignored frame isn't culled from
    the report. It represents overhead and should be kept"""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz'),
                   ('chrome', 'base::MessagePumpNSRunLoop::DoRun')],
        'weight':
        24
    }]
    shortened_stack = [{
        'frames': [('chrome', 'base::MessagePumpNSRunLoop::DoRun')],
        'weight':
        24
    }]

    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)
    stack_collapser.ShortenStackSamples()
    self.assertEquals(shortened_stack,
                      stack_collapser.GetSamplesListForTesting())

  def testStackShortening(self):
    """Tests that stacks get shortened to drop frames of low interest."""

    samples = [{
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz')],
        'weight':
        24
    }, {
        'frames': [('fake_module', 'foo'), ('fake_module', 'bar'),
                   ('fake_module', 'baz'),
                   ('chrome', 'base::MessagePumpNSRunLoop::DoRun')],
        'weight':
        24
    }]

    # Stack will be shortened to remove anything before the uninteresting frame.
    shortened_samples = samples.copy()
    shortened_samples[1]["frames"] = shortened_samples[1]["frames"][3:]

    stack_collapser = DTraceParser()
    stack_collapser.AddSamplesForTesting(samples)
    stack_collapser.ShortenStackSamples()
    self.assertEquals(shortened_samples,
                      stack_collapser.GetSamplesListForTesting())

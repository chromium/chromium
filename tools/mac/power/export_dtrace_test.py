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
            ('unsymbolized module', '0x21'),
            ('unsymbolized module', '+0x85'),
            ('unsymbolized module', '0x37'),
        ],
        'weight':
        12
    }])


class StackCollapseTest(unittest.TestCase):
  def testDoublePostProcessStackSamplesFails(self):
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
      stack_collapser.PostProcessStackSamples()
      stack_collapser.PostProcessStackSamples()

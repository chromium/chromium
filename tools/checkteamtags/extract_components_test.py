# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
import unittest.mock as mock

from io import StringIO

import extract_components


class ExtractComponentsTest(unittest.TestCase):
  def setUp(self):
    super().setUp()
    self.maxDiff = None

  @mock.patch('sys.argv', ['extract_components', 'src'])
  def testBaseCase(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'team': 'other-dummy-team@chromium.org',
            'component': 'Components>Component2',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        error_code = extract_components.main()
      self.assertEqual(0, error_code)
      result_minus_readme = json.loads(saved_output.getvalue())
      del result_minus_readme['AAA-README']
      self.assertEqual(result_minus_readme, {
          'component-to-team': {
              'Components>Component2': 'other-dummy-team@chromium.org',
              'Dummy>Component': 'dummy-team@chromium.org',
              'Dummy>Component>Subcomponent':
                  'dummy-specialist-team@chromium.org'
          },
          'dir-to-component': {
              'dummydir1': 'Dummy>Component',
              'dummydir1/innerdir1':
                  'Dummy>Component>Subcomponent',
              'dummydir2': 'Components>Component2',
          },
          'dir-to-team': {
              'dummydir1': 'dummy-team@chromium.org',
              'dummydir1/innerdir1': 'dummy-specialist-team@chromium.org',
              'dummydir2': 'other-dummy-team@chromium.org',
          },
          'teams-per-component': {
              'Components>Component2': ['other-dummy-team@chromium.org'],
              'Dummy>Component': ['dummy-team@chromium.org'],
              'Dummy>Component>Subcomponent':
                  ['dummy-specialist-team@chromium.org'],
          }})

  @mock.patch('sys.argv', ['extract_components', 'src'])
  def testOsTagBreaksDuplication(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'team': 'mac-dummy-team@chromium.org',
            'component': 'Dummy>Component',
            'os': 'Mac'
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        error_code = extract_components.main()
      self.assertEqual(0, error_code)
      result_minus_readme = json.loads(saved_output.getvalue())
      del result_minus_readme['AAA-README']
      self.assertEqual(result_minus_readme, {
          'component-to-team': {
              'Dummy>Component': 'dummy-team@chromium.org',
              'Dummy>Component(Mac)': 'mac-dummy-team@chromium.org',
              'Dummy>Component>Subcomponent':
                  'dummy-specialist-team@chromium.org'
          },
          'dir-to-component': {
              'dummydir1': 'Dummy>Component',
              'dummydir1/innerdir1': 'Dummy>Component>Subcomponent',
              'dummydir2': 'Dummy>Component(Mac)'
          },
          'dir-to-team': {
              'dummydir1': 'dummy-team@chromium.org',
              'dummydir1/innerdir1': 'dummy-specialist-team@chromium.org',
              'dummydir2': 'mac-dummy-team@chromium.org',
          },
          'teams-per-component': {
              'Dummy>Component': ['dummy-team@chromium.org'],
              'Dummy>Component(Mac)': ['mac-dummy-team@chromium.org'],
              'Dummy>Component>Subcomponent': [
                  'dummy-specialist-team@chromium.org']
          }})

  @mock.patch('sys.argv', ['extract_components', 'src'])
  def testMultipleTeamsOneComponent(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'team': 'other-dummy-team@chromium.org',
            'component': 'Dummy>Component2',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        error_code = extract_components.main()
      self.assertEqual(0, error_code)
      result_minus_readme = json.loads(saved_output.getvalue())
      del result_minus_readme['AAA-README']
      self.assertEqual(result_minus_readme, {
          'component-to-team': {
              'Dummy>Component': 'dummy-team@chromium.org',
              'Dummy>Component2':
                  'other-dummy-team@chromium.org'
          },
          'dir-to-component': {
              'dummydir1': 'Dummy>Component',
              'dummydir1/innerdir1': 'Dummy>Component',
              'dummydir2': 'Dummy>Component2'
          },
          'dir-to-team': {
              'dummydir1': 'dummy-team@chromium.org',
              'dummydir1/innerdir1': 'dummy-specialist-team@chromium.org',
              'dummydir2': 'other-dummy-team@chromium.org',
          },
          'teams-per-component': {
              'Dummy>Component': [
                  'dummy-specialist-team@chromium.org',
                  'dummy-team@chromium.org'],
              'Dummy>Component2': [
                  'other-dummy-team@chromium.org'],
          }})

  @mock.patch('sys.argv', ['extract_components', '-v', 'src'])
  def testVerbose(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'team': 'other-dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        extract_components.main()
      output = saved_output.getvalue()
      self.assertIn('OWNERS has no COMPONENT tag', output)

  @mock.patch('sys.argv', ['extract_components', '-s 2', 'src'])
  def testCoverage(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'component': 'Dummy>Component',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        extract_components.main()
      output = saved_output.getvalue()
      self.assertIn('4 OWNERS files in total.', output)
      self.assertIn('3 (75.00%) OWNERS files have COMPONENT', output)
      self.assertIn('2 (50.00%) OWNERS files have TEAM and COMPONENT', output)

  @mock.patch('sys.argv', ['extract_components', '-c', ''])
  def testCompleteCoverage(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir2': {
            'component': 'Dummy>Component',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        extract_components.main()
      output = saved_output.getvalue()
      self.assertIn('4 OWNERS files in total.', output)
      self.assertIn('3 (75.00%) OWNERS files have COMPONENT', output)
      self.assertIn('2 (50.00%) OWNERS files have TEAM and COMPONENT', output)
      self.assertIn('1 OWNERS files at depth 0', output)
      self.assertIn('2 OWNERS files at depth 1', output)
      self.assertIn('1 OWNERS files at depth 2', output)

  @mock.patch('sys.argv', ['extract_components', '-m 2', 'src'])
  def testDisplayFile(self):
    with mock.patch('extract_components.scrape_owners', return_value={
        '.': {},
        'dummydir1': {
            'team': 'dummy-team@chromium.org',
            'component': 'Dummy>Component',
        },
        'dummydir1/innerdir1': {
            'team': 'dummy-specialist-team@chromium.org',
            'component': 'Dummy>Component>Subcomponent'
        }
    }):
      saved_output = StringIO()
      with mock.patch('sys.stdout', saved_output):
        extract_components.main()
      output = saved_output.getvalue()
      self.assertIn('OWNERS files that have missing team and component '
                    'by depth:', output)
      self.assertIn('at depth 0', output)
      self.assertIn('[\'OWNERS\']', output)

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import fieldtrial_util
import os
import tempfile


class FieldTrialUtilUnittest(unittest.TestCase):

  def runGenerateArgs(self, config, platform, override_args=None):
    result = None
    with tempfile.NamedTemporaryFile('w', delete=False) as base_file:
      try:
        base_file.write(config)
        base_file.close()
        result = fieldtrial_util.GenerateArgs(base_file.name, platform,
                                              override_args)
      finally:
        os.unlink(base_file.name)
    return result

  def test_GenArgsEmptyPaths(self):
    args = fieldtrial_util.GenerateArgs('', 'linux')
    self.assertEqual([], args)

  def test_GenArgsOneConfig(self):
    config = '''{
      "BrowserBlackList": [
        {
          "platforms": ["windows"],
          "experiments": [{"name": "Enabled"}]
        }
      ],
      "SimpleParams": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "params": {"id": "abc"},
              "enable_features": ["a", "b"]
            }
          ]
        }
      ],
      "c": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "d.",
              "params": {"url": "http://www.google.com"},
              "enable_features": ["x"],
              "disable_features": ["y"]
            }
          ]
        }
      ]
    }'''
    result = self.runGenerateArgs(config, 'windows')
    self.assertEqual(['--force-fieldtrials='
        'BrowserBlackList/Enabled/SimpleParams/Default/c/d.',
        '--force-fieldtrial-params='
        'SimpleParams.Default:id/abc,'
        'c.d%2E:url/http%3A%2F%2Fwww%2Egoogle%2Ecom',
        '--enable-features=a<SimpleParams,b<SimpleParams,x<c',
        '--disable-features=y<c'], result)

  def test_GenArgsDuplicateEnableFeatures(self):
    config = '''{
      "X": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "x",
              "enable_features": ["x"]
            }
          ]
        }
      ],
      "Y": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "enable_features": ["x", "y"]
            }
          ]
        }
      ]
    }'''
    with self.assertRaises(Exception) as raised:
      self.runGenerateArgs(config, 'windows')
    self.assertEqual('Duplicate feature(s) in enable_features: x',
                     str(raised.exception))

  def test_GenArgsDuplicateDisableFeatures(self):
    config = '''{
      "X": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "x",
              "enable_features": ["y", "z"]
            }
          ]
        }
      ],
      "Y": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "enable_features": ["z", "x", "y"]
            }
          ]
        }
      ]
    }'''
    with self.assertRaises(Exception) as raised:
      self.runGenerateArgs(config, 'windows')
    self.assertEqual('Duplicate feature(s) in enable_features: y, z',
                     str(raised.exception))


  def test_GenArgsDuplicateEnableDisable(self):
    config = '''{
      "X": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "x",
              "enable_features": ["x"]
            }
          ]
        }
      ],
      "Y": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "disable_features": ["x", "y"]
            }
          ]
        }
      ]
    }'''
    with self.assertRaises(Exception) as raised:
      self.runGenerateArgs(config, 'windows')
    self.assertEqual('Conflicting features set as both enabled and disabled: x',
                     str(raised.exception))

  def test_GenArgsOverrideArgs(self):
    config = '''{
      "BrowserBlackList": [
        {
          "platforms": ["windows"],
          "experiments": [{"name": "Enabled"}]
        }
      ],
      "SimpleParams": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "params": {"id": "abc"},
              "enable_features": ["a", "b"]
            }
          ]
        }
      ],
      "MoreParams": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "Default",
              "params": {"id": "abc"},
              "enable_features": ["aa", "bb", "qq"]
            }
          ]
        }
      ],
      "c": [
        {
          "platforms": ["windows"],
          "experiments": [
            {
              "name": "d.",
              "params": {"url": "http://www.google.com"},
              "enable_features": ["x"],
              "disable_features": ["y"]
            }
          ]
        }
      ]
    }'''
    result = self.runGenerateArgs(
        config, 'windows', ['--enable-features=y', '--disable-features=qq'])
    self.assertEqual(['--force-fieldtrials='
        'BrowserBlackList/Enabled/SimpleParams/Default',
        '--force-fieldtrial-params='
        'SimpleParams.Default:id/abc',
        '--enable-features=a<SimpleParams,b<SimpleParams'], result)

  def test_MergeArgsEmpty(self):
    args = fieldtrial_util.MergeFeaturesAndFieldTrialsArgs([])
    self.assertEqual([], args)

  def test_MergeArgsRepeats(self):
    args = fieldtrial_util.MergeFeaturesAndFieldTrialsArgs([
        '--disable-features=Feature1,Feature2',
        '--disable-features=Feature2,Feature3',
        '--enable-features=Feature4,Feature5',
        '--enable-features=Feature5,Feature6',
        '--foo',
        '--force-fieldtrials=Group1/Exp1/Group2/Exp2',
        '--force-fieldtrials=Group3/Exp3/Group4/Exp4',
        '--force-fieldtrial-params=Group1.Exp1:id/abc,Group2.Exp2:id/bcd',
        '--force-fieldtrial-params=Group4.Exp4:id/cde',
        '--bar'])

    # For each flag, we expect alphabetical ordering of the pieces merged as
    # they are sorted first.
    self.assertEquals(args, [
        '--foo',
        '--bar',
        '--disable-features=Feature1,Feature2,Feature3',
        '--enable-features=Feature4,Feature5,Feature6',
        '--force-fieldtrials=Group1/Exp1/Group2/Exp2/Group3/Exp3/Group4/Exp4',
        '--force-fieldtrial-params=Group1.Exp1:id/abc,Group2.Exp2:id/bcd,'
        + 'Group4.Exp4:id/cde',
    ])

if __name__ == '__main__':
  unittest.main()

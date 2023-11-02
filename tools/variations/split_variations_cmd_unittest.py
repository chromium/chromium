# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import os
import split_variations_cmd

_ENABLE_FEATURES_SWITCH_NAME = 'enable-features'
_DISABLE_FEATURES_SWITCH_NAME = 'disable-features'
_FORCE_FIELD_TRIALS_SWITCH_NAME = 'force-fieldtrials'
_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME = 'force-fieldtrial-params'

class SplitVariationsCmdUnittest(unittest.TestCase):

  def _CompareCommandLineSwitches(self, filename, cmd_list):
    """Compares two sets of command line switches.

    Args:
        filename: Name to a file that contains a set of commandline switches.
        cmd_list: A list of strings in the form of '--switch_name=switch_value'.

    Return True if they contain the same switches and each switch's values
    are the same.
    """
    assert os.path.isfile(filename)
    data = None
    with open(filename, 'r') as f:
      data = f.read().replace('\n', ' ')
    switches = split_variations_cmd.ParseCommandLineSwitchesString(data)
    if len(switches) != len(cmd_list):
      return False
    for switch_name, switch_value in switches.items():
      switch_string = '--%s="%s"' % (switch_name, switch_value)
      if switch_string not in cmd_list:
        return False
    return True

  def _GetUnittestDataDir(self):
    return os.path.join(os.path.dirname(__file__), 'unittest_data')

  def _VerifySplits(self, switch_name, splits, ref_switch_data):
    """Verifies splitting commandline switches works correctly.

    Compare that when we combine switch data from all |splits| into one,
    it's exactly the same as the |ref_switch_data|. Also check the splits are
    almost evenly distributed, that is, their data size are almost the same.

    Args:
        switch_name: The name of the switch that is verified.
        splits: A list of {switch_name: [items]} dictionaries.
                Each list element represents one of the split switch sets.
                |items| is a list of items representing switch value.
        ref_switch_data: A {switch_name: [items]} dictionary.
                         This is the switch set before splitting.
                         |items| is a list of items representing switch value.
    """
    data_lists = [
        split[switch_name] for split in splits if switch_name in split]
    if len(data_lists) == 0:
      self.assertFalse(ref_switch_data)
      return
    max_size = max(len(data) for data in data_lists)
    min_size = min(len(data) for data in data_lists)
    if switch_name != _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME:
      self.assertTrue(max_size - min_size <= 1)
    joined_switch_data = []
    for data in data_lists:
      joined_switch_data.extend(data)
    self.assertEqual(ref_switch_data, joined_switch_data)


  def testLoadFromFileAndSaveToStrings(self):
    # Verifies we load data from the file and save it to a list of strings,
    # the two data sets contain the same command line switches.
    data_file = os.path.join(self._GetUnittestDataDir(), 'variations_cmd.txt')
    assert os.path.isfile(data_file)
    data = split_variations_cmd.ParseVariationsCmdFromFile(data_file)
    cmd_list = split_variations_cmd.VariationsCmdToStrings(data)
    self.assertTrue(self._CompareCommandLineSwitches(data_file, cmd_list))


  def _testSplitVariationsCmdHelper(self, input_data):
    # Verifies we correctly and (almost) evenly split one set of command line
    # switches into two sets.
    splits = split_variations_cmd.SplitVariationsCmd(input_data)
    switches = [_ENABLE_FEATURES_SWITCH_NAME,
                _DISABLE_FEATURES_SWITCH_NAME,
                _FORCE_FIELD_TRIALS_SWITCH_NAME,
                _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME]
    for switch in switches:
      self._VerifySplits(switch, splits, input_data.get(switch, []))
    # Verify both split variations are valid.
    for variations_cmd in splits:
      cmd_list = split_variations_cmd.VariationsCmdToStrings(variations_cmd)
      split_variations_cmd.ParseVariationsCmdFromString(' '.join(cmd_list))


  def testSplitVariationsCmd(self):
    input_file = os.path.join(self._GetUnittestDataDir(), 'variations_cmd.txt')
    assert os.path.isfile(input_file)
    data = split_variations_cmd.ParseVariationsCmdFromFile(input_file)
    self._testSplitVariationsCmdHelper(data)


  def testSplitVariationsCmdWithMissingEnableDisableFeatures(self):
    input_string = (
        '--force-fieldtrials="Tria1/Disabled/*Trial2/Enabled/" '
        '--force-fieldtrial-params="Trial2.Enabled:age/18/gender/male" '
        '--disable-features="FeatureA<FeatureA"')
    data = split_variations_cmd.ParseVariationsCmdFromString(input_string)
    self._testSplitVariationsCmdHelper(data)


  def testSplitVariationsCmdWithMissingForceFieldTrialParams(self):
    input_string = (
        '--force-fieldtrials="*Trial2/Enabled/" '
        '--enable-features="FeatureA<FeatureA,FeatureB<FeatureB" '
        '--disable-features="FeatureC<FeatureC,FeatureD<FeatureD"')
    data = split_variations_cmd.ParseVariationsCmdFromString(input_string)
    self._testSplitVariationsCmdHelper(data)

  def testSplitVariationsCmdNoFurtherSplit(self):
    input_string = (
        '--force-fieldtrials="*Trial2/Enabled/" '
        '--enable-features="FeatureA<FeatureA" '
        '--disable-features="FeatureC<FeatureC"')
    splits = split_variations_cmd.SplitVariationsCmdFromString(input_string)
    self.assertEqual(1, len(splits))


if __name__ == '__main__':
  unittest.main()

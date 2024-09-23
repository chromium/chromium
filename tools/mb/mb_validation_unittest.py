#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for mb_validate.py."""

import sys
import ast
import os
import unittest

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..'))

from mb import mb
from mb import mb_unittest
from mb.lib import validation

TEST_UNREFERENCED_MIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel'],
    'rel_bot_2': ['rel'],
  },
  'builder_groups': {
    'fake_builder_group_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'unreferenced_mixin': {
      'gn_args': 'proprietary_codecs=true',
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TEST_UNKNOWNMIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel'],
    'rel_bot_2': ['rel', 'unknown_mixin'],
  },
  'builder_groups': {
    'fake_builder_group_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TEST_UNKNOWN_NESTED_MIXIN_CONFIG = """\
{
  'public_artifact_builders': {},
  'configs': {
    'rel_bot_1': ['rel', 'nested_mixin'],
    'rel_bot_2': ['rel'],
  },
  'builder_groups': {
    'fake_builder_group_a': {
      'fake_builder_a': 'rel_bot_1',
      'fake_builder_b': 'rel_bot_2',
    },
  },
  'mixins': {
    'nested_mixin': {
      'mixins': {
        'unknown_mixin': {
          'gn_args': 'proprietary_codecs=true',
        },
      },
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TEST_CONFIG_UNSORTED_GROUPS = """\
{
  'builder_groups': {
    'groupB': {},
    'groupA': {},
    'groupC': {},
  },
  'configs': {
  },
  'mixins': {
  },
}
"""

TEST_CONFIG_UNSORTED_BUILDERNAMES = """\
{
  'builder_groups': {
    'group': {
      'builderB': '',
      'builderA': ''
    },
  },
  'configs': {
  },
  'mixins': {
  },
}
"""

TEST_CONFIG_UNSORTED_CONFIGS = """\
{
  'builder_groups': {
  },
  'configs': {
    'configB': {},
    'configA': {},
  },
  'mixins': {
  },
}
"""

TEST_CONFIG_UNSORTED_MIXINS = """\
{
  'builder_groups': {
  },
  'configs': {
  },
  'mixins': {
    'mixinB': {},
    'mixinA': {},
  },
}
"""


class UnitTest(unittest.TestCase):
  def test_GetAllConfigs(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG)
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    self.assertEqual(all_configs['rel_bot'], 'fake_builder_group')
    self.assertEqual(all_configs['debug_remoteexec'], 'fake_builder_group')

  def test_CheckAllConfigsAndMixinsReferenced_ok(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertEqual(errs, [])

  def test_CheckAllConfigsAndMixinsReferenced_unreferenced(self):
    configs = ast.literal_eval(TEST_UNREFERENCED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn('Unreferenced mixin "unreferenced_mixin".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unknown(self):
    configs = ast.literal_eval(TEST_UNKNOWNMIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)
    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by config "rel_bot_2".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unknown_nested(self):
    configs = ast.literal_eval(TEST_UNKNOWN_NESTED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by mixin "nested_mixin".', errs)

  def test_CheckAllConfigsAndMixinsReferenced_unused(self):
    configs = ast.literal_eval(TEST_UNKNOWN_NESTED_MIXIN_CONFIG)
    errs = []
    all_configs = validation.GetAllConfigs(configs['builder_groups'])
    config_configs = configs['configs']
    mixins = configs['mixins']

    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  config_configs, mixins)

    self.assertIn(
        'Unknown mixin "unknown_mixin" '
        'referenced by mixin "nested_mixin".', errs)

  def test_CheckDuplicateConfigs_ok(self):
    configs = ast.literal_eval(mb_unittest.TEST_CONFIG)
    config_configs = configs['configs']
    mixins = configs['mixins']
    grouping = configs['builder_groups']
    errs = []

    validation.CheckDuplicateConfigs(errs, config_configs, mixins, grouping,
                                     mb.FlattenConfig)
    self.assertEqual(errs, [])

  @unittest.skip('bla')
  def test_CheckDuplicateConfigs_dups(self):
    configs = ast.literal_eval(mb_unittest.TEST_DUP_CONFIG)
    config_configs = configs['configs']
    mixins = configs['mixins']
    grouping = configs['builder_groups']
    errs = []

    validation.CheckDuplicateConfigs(errs, config_configs, mixins, grouping,
                                     mb.FlattenConfig)
    self.assertIn(
        'Duplicate configs detected. When evaluated fully, the '
        'following configs are all equivalent: \'some_config\', '
        '\'some_other_config\'. Please consolidate these configs '
        'into only one unique name per configuration value.', errs)

  def test_CheckKeyOrderingOK(self):
    mb_config = ast.literal_eval(mb_unittest.TEST_CONFIG)
    errs = []
    validation.CheckKeyOrdering(errs, mb_config['builder_groups'],
                                mb_config['configs'], mb_config['mixins'])
    self.assertEqual(errs, [])

  def test_CheckKeyOrderingBad(self):
    mb_config = ast.literal_eval(TEST_CONFIG_UNSORTED_GROUPS)
    errs = []
    validation.CheckKeyOrdering(errs, mb_config['builder_groups'],
                                mb_config['configs'], mb_config['mixins'])
    self.assertIn('\nThe keys in "builder_groups" are not sorted:', errs)

    mb_config = ast.literal_eval(TEST_CONFIG_UNSORTED_BUILDERNAMES)
    errs = []
    validation.CheckKeyOrdering(errs, mb_config['builder_groups'],
                                mb_config['configs'], mb_config['mixins'])
    self.assertIn('\nThe builders in group "group" are not sorted:', errs)

    mb_config = ast.literal_eval(TEST_CONFIG_UNSORTED_CONFIGS)
    errs = []
    validation.CheckKeyOrdering(errs, mb_config['builder_groups'],
                                mb_config['configs'], mb_config['mixins'])
    self.assertIn('\nThe config names are not sorted:', errs)

    mb_config = ast.literal_eval(TEST_CONFIG_UNSORTED_MIXINS)
    errs = []
    validation.CheckKeyOrdering(errs, mb_config['builder_groups'],
                                mb_config['configs'], mb_config['mixins'])
    self.assertIn('\nThe mixin names are not sorted:', errs)


if __name__ == '__main__':
  unittest.main()

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validation functions for the Meta-Build config file"""

import ast
import collections
import json
import os
import re


def GetAllConfigs(masters):
  """Build a list of all of the configs referenced by builders.
  """
  all_configs = {}
  for master in masters:
    for config in masters[master].values():
      if isinstance(config, dict):
        for c in config.values():
          all_configs[c] = master
      else:
        all_configs[config] = master
  return all_configs


def CheckAllConfigsAndMixinsReferenced(errs, all_configs, configs, mixins):
  """Check that every actual config is actually referenced."""
  for config in configs:
    if not config in all_configs:
      errs.append('Unused config "%s".' % config)

  # Figure out the whole list of mixins, and check that every mixin
  # listed by a config or another mixin actually exists.
  referenced_mixins = set()
  for config, mixin_names in configs.items():
    for mixin in mixin_names:
      if not mixin in mixins:
        errs.append(
            'Unknown mixin "%s" referenced by config "%s".' % (mixin, config))
      referenced_mixins.add(mixin)

  for mixin in mixins:
    for sub_mixin in mixins[mixin].get('mixins', []):
      if not sub_mixin in mixins:
        errs.append(
            'Unknown mixin "%s" referenced by mixin "%s".' % (sub_mixin, mixin))
      referenced_mixins.add(sub_mixin)

  # Check that every mixin defined is actually referenced somewhere.
  for mixin in mixins:
    if not mixin in referenced_mixins:
      errs.append('Unreferenced mixin "%s".' % mixin)

  return errs


def EnsureNoProprietaryMixins(errs, masters, configs, mixins):
  """If we're checking the Chromium config, check that the 'chromium' bots
  which build public artifacts do not include the chrome_with_codecs mixin.
  """
  if 'chromium' in masters:
    for builder in masters['chromium']:
      config = masters['chromium'][builder]

      def RecurseMixins(current_mixin):
        if current_mixin == 'chrome_with_codecs':
          errs.append('Public artifact builder "%s" can not contain the '
                      '"chrome_with_codecs" mixin.' % builder)
          return
        if not 'mixins' in mixins[current_mixin]:
          return
        for mixin in mixins[current_mixin]['mixins']:
          RecurseMixins(mixin)

      for mixin in configs[config]:
        RecurseMixins(mixin)
  else:
    errs.append('Missing "chromium" master. Please update this '
                'proprietary codecs check with the name of the master '
                'responsible for public build artifacts.')


def _GetConfigsByBuilder(masters):
  """Builds a mapping from buildername -> [config]

    Args
      masters: the master's dict from mb_config.pyl
    """

  result = collections.defaultdict(list)
  for master in masters.values():
    for buildername, builder in master.items():
      result[buildername].append(builder)

  return result


def CheckDuplicateConfigs(errs, config_pool, mixin_pool, grouping,
                          flatten_config):
  """Check for duplicate configs.

  Evaluate all configs, and see if, when
  evaluated, differently named configs are the same.
  """
  evaled_to_source = collections.defaultdict(set)
  for group, builders in grouping.items():
    for builder in builders:
      config = grouping[group][builder]
      if not config:
        continue

      if isinstance(config, dict):
        # Ignore for now
        continue
      elif config.startswith('//'):
        args = config
      else:
        flattened_config = flatten_config(config_pool, mixin_pool, config)
        args = flattened_config['gn_args']
        if 'error' in args:
          continue
        # Force the args_file into consideration when testing for duplicate
        # configs.
        args_file = flattened_config['args_file']
        if args_file:
          args += ' args_file=%s' % args_file

      evaled_to_source[args].add(config)

  for v in evaled_to_source.values():
    if len(v) != 1:
      errs.append(
          'Duplicate configs detected. When evaluated fully, the '
          'following configs are all equivalent: %s. Please '
          'consolidate these configs into only one unique name per '
          'configuration value.' % (', '.join(sorted('%r' % val for val in v))))


def CheckExpectations(mbw, jsonish_blob, expectations_dir):
  """Checks that the expectation files match the config file.

  Returns: True if expectations are up-to-date. False otherwise.
  """
  # Assert number of masters == number of expectation files.
  if len(mbw.ListDir(expectations_dir)) != len(jsonish_blob):
    return False
  for master, builders in jsonish_blob.items():
    if not mbw.Exists(os.path.join(expectations_dir, master + '.json')):
      return False  # No expecation file for the master.
    expectation = mbw.ReadFile(os.path.join(expectations_dir, master + '.json'))
    builders_json = json.dumps(builders,
                               indent=2,
                               sort_keys=True,
                               separators=(',', ': '))
    if builders_json != expectation:
      return False  # Builders' expectation out of sync.
  return True

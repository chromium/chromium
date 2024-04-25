# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validation functions for the Meta-Build config file"""

import ast
import collections
import difflib
import json
import os
import re


def GetAllConfigs(builder_groups):
  """Build a list of all of the configs referenced by builders.
  """
  all_configs = {}
  for builder_group in builder_groups:
    for config in builder_groups[builder_group].values():
      if isinstance(config, dict):
        for c in config.values():
          all_configs[c] = builder_group
      else:
        all_configs[config] = builder_group
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


def _GetConfigsByBuilder(builder_groups):
  """Builds a mapping from buildername -> [config]

    Args
      builder_groups: the builder_group's dict from mb_config.pyl
    """

  result = collections.defaultdict(list)
  for builder_group in builder_groups.values():
    for buildername, builder in builder_group.items():
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

      if config.startswith('//'):
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


def CheckDebugDCheckOrOfficial(errs, gn_args, builder_group, builder, phase):
  # TODO(crbug.com/40189120): Figure out how to check this properly
  # for simplechrome-based bots.
  if gn_args.get('is_chromeos_device'):
    return

  if ((gn_args.get('is_debug') == True)
      or (gn_args.get('is_official_build') == True)
      or ('dcheck_always_on' in gn_args)):
    return

  if phase:
    errs.append('Phase "%s" of builder "%s" on %s did not specify '
                'one of is_debug=true, is_official_build=true, or '
                'dcheck_always_on=(true|false).' %
                (phase, builder, builder_group))
  else:
    errs.append('Builder "%s" on %s did not specify '
                'one of is_debug=true, is_official_build=true, or '
                'dcheck_always_on=(true|false).' % (builder, builder_group))


def CheckExpectations(mbw, jsonish_blob, expectations_dir):
  """Checks that the expectation files match the config file.

  Returns: True if expectations are up-to-date. False otherwise.
  """
  # Assert number of builder_groups == number of expectation files.
  if len(mbw.ListDir(expectations_dir)) != len(jsonish_blob):
    return False
  for builder_group, builders in jsonish_blob.items():
    if not mbw.Exists(os.path.join(expectations_dir, builder_group + '.json')):
      return False  # No expecation file for the builder_group.
    expectation = mbw.ReadFile(os.path.join(expectations_dir,
                                            builder_group + '.json'))
    builders_json = json.dumps(builders,
                               indent=2,
                               sort_keys=True,
                               separators=(',', ': '))
    if builders_json != expectation:
      return False  # Builders' expectation out of sync.
  return True


def CheckKeyOrdering(errs, groups, configs, mixins):
  # Check ordering of groups within "builder_groups".
  group_names = list(groups.keys())
  sorted_group_names = sorted(group_names)
  if group_names != sorted_group_names:
    errs.append('\nThe keys in "builder_groups" are not sorted:')
    errs.extend(difflib.context_diff(group_names, sorted_group_names))

  # Check ordering of builders within each group.
  for group, builders in groups.items():
    builder_names = list(builders.keys())
    sorted_builder_names = sorted(builder_names)
    if builder_names != sorted_builder_names:
      errs.append('\nThe builders in group "%s" are not sorted:' % group)
      errs.extend(difflib.context_diff(builder_names, sorted_builder_names))

  # Check ordering of configs names, but don't bother checking the ordering
  # of mixins within a config.
  config_names = list(configs.keys())
  sorted_config_names = sorted(config_names)
  if config_names != sorted_config_names:
    errs.append('\nThe config names are not sorted:')
    errs.extend(difflib.context_diff(config_names, sorted_config_names))

  # Check ordering of mixin names.
  mixin_names = list(mixins.keys())
  sorted_mixin_names = sorted(mixin_names)
  if mixin_names != sorted_mixin_names:
    errs.append('\nThe mixin names are not sorted:')
    errs.extend(difflib.context_diff(mixin_names, sorted_mixin_names))

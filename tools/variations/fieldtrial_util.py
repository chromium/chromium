# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import sys

import fieldtrial_to_struct

def _hex(ch):
  hv = hex(ord(ch)).replace('0x', '')
  hv.zfill(2)
  return hv.upper()

# URL escapes the delimiter characters from the output. urllib.quote is not
# used because it cannot escape '.'.
def _escape(str):
  result = str
  # Must perform replace on '%' first before the others.
  for c in '%:/.,':
    result = result.replace(c, '%' + _hex(c))
  return result

def _FindDuplicates(entries):
  seen = set()
  duplicates = set()
  for entry in entries:
    if entry in seen:
      duplicates.add(entry)
    else:
      seen.add(entry)
  return sorted(duplicates)

def _CheckForDuplicateFeatures(enable_features, disable_features):
  enable_features = [f.split('<')[0] for f in enable_features]
  enable_features_set = set(enable_features)
  if len(enable_features_set) != len(enable_features):
    raise Exception('Duplicate feature(s) in enable_features: ' +
                    ', '.join(_FindDuplicates(enable_features)))

  disable_features = [f.split('<')[0] for f in disable_features]
  disable_features_set = set(disable_features)
  if len(disable_features_set) != len(disable_features):
    raise Exception('Duplicate feature(s) in disable_features: ' +
                    ', '.join(_FindDuplicates(disable_features)))

  features_in_both = enable_features_set.intersection(disable_features_set)
  if len(features_in_both) > 0:
    raise Exception('Conflicting features set as both enabled and disabled: ' +
                    ', '.join(features_in_both))

def _FindFeaturesOverriddenByArgs(args):
  """Returns a list of the features enabled or disabled by the flags in args."""
  overridden_features = []
  for arg in args:
    if (arg.startswith('--enable-features=')
        or arg.startswith('--disable-features=')):
      _, _, arg_val = arg.partition('=')
      overridden_features.extend(arg_val.split(','))
  return [f.split('<')[0] for f in overridden_features]

def MergeFeaturesAndFieldTrialsArgs(args):
  """Merges duplicate features and field trials arguments.

  Merges multiple instances of --enable-features, --disable-features,
  --force-fieldtrials and --force-fieldtrial-params. Any such merged flags are
  moved to the end of the returned list. The original argument ordering is
  otherwise maintained.
  TODO(crbug.com/40663174): Add functionality to handle duplicate flags using
  the Foo<Bar syntax. Currently, the implementation considers e.g. 'Foo',
  'Foo<Bar' and 'Foo<Baz' to be different. Also add functionality to handle
  cases where the same trial is specified with different groups via
  --force-fieldtrials, which isn't currently unhandled.

  Args:
    args: An iterable of strings representing command line arguments.

  Returns:
    A new list of strings representing the merged command line arguments.
  """
  merged_args = []
  disable_features = set()
  enable_features = set()
  force_fieldtrials = set()
  force_fieldtrial_params = set()
  for arg in args:
    if arg.startswith('--disable-features='):
      disable_features.update(arg.split('=', 1)[1].split(','))
    elif arg.startswith('--enable-features='):
      enable_features.update(arg.split('=', 1)[1].split(','))
    elif arg.startswith('--force-fieldtrials='):
      # A trailing '/' is optional. Do not split by '/' as that would separate
      # each group name from the corresponding trial name.
      force_fieldtrials.add(arg.split('=', 1)[1].rstrip('/'))
    elif arg.startswith('--force-fieldtrial-params='):
      force_fieldtrial_params.update(arg.split('=', 1)[1].split(','))
    else:
      merged_args.append(arg)

  # Sort arguments to ensure determinism.
  if disable_features:
    merged_args.append('--disable-features=%s' % ','.join(
        sorted(disable_features)))
  if enable_features:
    merged_args.append('--enable-features=%s' % ','.join(
        sorted(enable_features)))
  if force_fieldtrials:
    merged_args.append('--force-fieldtrials=%s' % '/'.join(
        sorted(force_fieldtrials)))
  if force_fieldtrial_params:
    merged_args.append('--force-fieldtrial-params=%s' % ','.join(
        sorted(force_fieldtrial_params)))

  return merged_args

def GenerateArgs(config_path, platform, override_args=None):
  """Generates command-line flags for enabling field trials.

  Generates a list of command-line switches to enable field trials for the
  provided config_path and platform. If override_args is set, all field trials
  that conflict with any listed --enable-features or --disable-features argument
  are skipped.

  Args:
    config_path: The path to the fieldtrial testing config JSON file.
    platform: A string representing the platform on which the tests will be run.
    override_args (optional): An iterable of string command line arguments.

  Returns:
    A list of string command-line arguments.
  """
  try:
    with open(config_path, 'r') as config_file:
      config = json.load(config_file)
  except (IOError, ValueError):
    return []

  platform_studies = fieldtrial_to_struct.ConfigToStudies(config, [platform])

  if override_args is None:
    override_args = []
  overriden_features_set = set(_FindFeaturesOverriddenByArgs(override_args))
  # Should skip any experiment that will enable or disable a feature that is
  # also enabled or disabled in the override_args.
  def ShouldSkipExperiment(experiment):
    experiment_features = (experiment.get('disable_features', [])
                           + experiment.get('enable_features', []))
    return not overriden_features_set.isdisjoint(experiment_features)

  studies = []
  params = []
  enable_features = []
  disable_features = []

  for study in platform_studies:
    study_name = study['name']
    experiments = study['experiments']
    # For now, only take the first experiment.
    experiment = experiments[0]
    if ShouldSkipExperiment(experiment):
      continue
    selected_study = [study_name, experiment['name']]
    studies.extend(selected_study)
    param_list = []
    if 'params' in experiment:
      for param in experiment['params']:
        param_list.append(param['key'])
        param_list.append(param['value'])
    if len(param_list):
      # Escape the variables for the command-line.
      selected_study = [_escape(x) for x in selected_study]
      param_list = [_escape(x) for x in param_list]
      param = '%s:%s' % ('.'.join(selected_study), '/'.join(param_list))
      params.append(param)
    for feature in experiment.get('enable_features', []):
      enable_features.append(feature + '<' + study_name)
    for feature in experiment.get('disable_features', []):
      disable_features.append(feature + '<' + study_name)

  if not len(studies):
    return []
  _CheckForDuplicateFeatures(enable_features, disable_features)
  args = ['--force-fieldtrials=%s' % '/'.join(studies)]
  if len(params):
    args.append('--force-fieldtrial-params=%s' % ','.join(params))
  if len(enable_features):
    args.append('--enable-features=%s' % ','.join(enable_features))
  if len(disable_features):
    args.append('--disable-features=%s' % ','.join(disable_features))
  return args

def main():
  if len(sys.argv) < 3:
    print('Usage: fieldtrial_util.py [config_path] [platform]')
    print('Optionally pass \'shell_cmd\' as an extra argument to print')
    print('quoted command line arguments.')
    exit(-1)
  print_shell_cmd = len(sys.argv) >= 4 and sys.argv[3] == 'shell_cmd'

  supported_platforms = ['android', 'android_webview', 'chromeos', 'ios',
                         'linux', 'mac', 'windows']
  if sys.argv[2] not in supported_platforms:
    print('\'%s\' is an unknown platform. Supported platforms: %s' %
          (sys.argv[2], supported_platforms))
    exit(-1)

  generated_args = GenerateArgs(sys.argv[1], sys.argv[2])
  if print_shell_cmd:
    print(" ".join(map((lambda arg: '"{0}"'.format(arg)), generated_args)))
  else:
    print(generated_args)


if __name__ == '__main__':
  main()

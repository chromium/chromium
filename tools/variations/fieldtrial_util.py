# Copyright 2015 The Chromium Authors. All rights reserved.
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
  return duplicates

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

# Generate a list of command-line switches to enable field trials for the
# provided config_path and platforms.
def GenerateArgs(config_path, platforms):
  try:
    with open(config_path, 'r') as config_file:
      config = json.load(config_file)
  except (IOError, ValueError):
    return []

  platform_studies = fieldtrial_to_struct.ConfigToStudies(config, platforms)

  studies = []
  params = []
  enable_features = []
  disable_features = []

  for study in platform_studies:
    study_name = study['name']
    experiments = study['experiments']
    # For now, only take the first experiment.
    experiment = experiments[0]
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

  generated_args = GenerateArgs(sys.argv[1], [sys.argv[2]])
  if print_shell_cmd:
    print(" ".join(map((lambda arg: '"{0}"'.format(arg)), generated_args)))
  else:
    print(generated_args)


if __name__ == '__main__':
  main()

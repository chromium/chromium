# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to split Chrome variations into two sets.

Chrome runs with many experiments and variations (field trials) that are
randomly selected based on a configuration from a server. They lead to
different code paths and different Chrome behaviors. When a bug is caused by
one of the experiments or variations, it is useful to be able to bisect into
the set and pin-point which one is responsible.

Go to chrome://version/?show-variations-cmd, at the bottom a few commandline
switches define the current experiments and variations Chrome runs with.

Sample use:

python split_variations_cmd.py --file="variations_cmd.txt" --output-dir=".\out"

"variations_cmd.txt" is the command line switches data saved from
chrome://version/?show-variations-cmd. This command splits them into two sets.
If needed, the script can run on one set to further divide until a single
experiment/variation is pin-pointed as responsible.

Note that on Windows, directly passing the command line switches taken from
chrome://version/?show-variations-cmd to Chrome in "Command Prompt" won't work.
This is because Chrome in "Command Prompt" doesn't seem to handle
--force-fieldtrials="value"; it only handles --force-fieldtrials=value.
Run Chrome through "Windows PowerShell" instead.
"""

import collections
import os
import optparse
import sys
import urllib

_ENABLE_FEATURES_SWITCH_NAME = 'enable-features'
_DISABLE_FEATURES_SWITCH_NAME = 'disable-features'
_FORCE_FIELD_TRIALS_SWITCH_NAME = 'force-fieldtrials'
_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME = 'force-fieldtrial-params'


_Trial = collections.namedtuple('Trial', ['star', 'trial_name', 'group_name'])
_Param = collections.namedtuple('Param', ['key', 'value'])
_TrialParams = collections.namedtuple('TrialParams',
                                      ['trial_name', 'group_name', 'params'])
_Feature = collections.namedtuple('Feature', ['star', 'key', 'value'])

def _ParseForceFieldTrials(data):
  """Parses --force-fieldtrials switch value string.

  The switch value string is parsed to a list of _Trial objects.
  """
  data = data.rstrip('/')
  items = data.split('/')
  if len(items) % 2 != 0:
    raise ValueError('odd number of items in --force-fieldtrials value')
  trial_names = items[0::2]
  group_names = items[1::2]
  results = []
  for trial_name, group_name in zip(trial_names, group_names):
    star = False
    if trial_name.startswith('*'):
      star = True
      trial_name = trial_name[1:]
    results.append(_Trial(star, trial_name, group_name))
  return results


def _BuildForceFieldTrialsSwitchValue(trials):
  """Generates --force-fieldtrials switch value string.

  This is the opposite of _ParseForceFieldTrials().

  Args:
      trials: A list of _Trial objects from which the switch value string
          is generated.
  """
  return ''.join('%s%s/%s/' % (
    '*' if trial.star else '',
    trial.trial_name,
    trial.group_name
  ) for trial in trials)


def _ParseForceFieldTrialParams(data):
  """Parses --force-fieldtrial-params switch value string.

  The switch value string is parsed to a list of _TrialParams objects.

  Format is trial_name.group_name:param0/value0/.../paramN/valueN.
  """
  items = data.split(',')
  results = []
  for item in items:
    tokens = item.split(':')
    if len(tokens) != 2:
      raise ValueError('Wrong format, expected trial_name.group_name:'
                       'p0/v0/.../pN/vN, got %s' % item)
    trial_group = tokens[0].split('.')
    if len(trial_group) != 2:
      raise ValueError('Wrong format, expected trial_name.group_name, '
                       'got %s' % tokens[0])
    trial_name = trial_group[0]
    if len(trial_name) == 0 or trial_name[0] == '*':
      raise ValueError('Wrong field trail params format: %s' % item)
    group_name = trial_group[1]
    params = tokens[1].split('/')
    if len(params) < 2 or len(params) % 2 != 0:
      raise ValueError('Field trial params should be param/value pairs %s' %
                       tokens[1])
    pairs = [
      _Param(key=params[i], value=params[i + 1])
      for i in range(0, len(params), 2)
    ]
    results.append(_TrialParams(trial_name, group_name, pairs))
  return results


def _BuildForceFieldTrialParamsSwitchValue(trials):
  """Generates --force-fieldtrial-params switch value string.

  This is the opposite of _ParseForceFieldTrialParams().

  Args:
      trials: A list of _TrialParams objects from which the switch value
          string is generated.
  """
  return ','.join('%s.%s:%s' % (
    trial.trial_name,
    trial.group_name,
    '/'.join('%s/%s' % (param.key, param.value) for param in trial.params),
  ) for trial in trials)


def _ValidateForceFieldTrialsAndParams(trials, params):
  """Checks if all params have corresponding field trials specified.

  |trials| comes from --force-fieldtrials switch, |params| comes from
  --force-fieldtrial-params switch.
  """
  if len(params) > len(trials):
    raise ValueError("params size (%d) larger than trials size (%d)" %
                     (len(params), len(trials)))
  trial_groups = {trial.trial_name: trial.group_name for trial in trials}
  for param in params:
    trial_name = urllib.unquote(param.trial_name)
    group_name = urllib.unquote(param.group_name)
    if trial_name not in trial_groups:
      raise ValueError("Fail to find trial_name %s in trials" % trial_name)
    if group_name != trial_groups[trial_name]:
      raise ValueError("group_name mismatch for trial_name %s, %s vs %s" %
                       (trial_name, group_name, trial_groups[trial_name]))


def _SplitFieldTrials(trials, trial_params):
  """Splits (--force-fieldtrials, --force-fieldtrial-params) pair to two pairs.

  Note that any list in the output pairs could be empty, depending on the
  number of elements in the input lists.
  """
  middle = (len(trials) + 1) // 2
  params = {urllib.unquote(trial.trial_name): trial for trial in trial_params}

  trials_first = trials[:middle]
  params_first = []
  for trial in trials_first:
    if trial.trial_name in params:
      params_first.append(params[trial.trial_name])

  trials_second = trials[middle:]
  params_second = []
  for trial in trials_second:
    if trial.trial_name in params:
      params_second.append(params[trial.trial_name])

  return [
    {_FORCE_FIELD_TRIALS_SWITCH_NAME: trials_first,
     _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME: params_first},
    {_FORCE_FIELD_TRIALS_SWITCH_NAME: trials_second,
     _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME: params_second},
  ]


def _ParseFeatureListString(data, is_disable):
  """Parses --enable/--disable-features switch value string.

  The switch value string is parsed to a list of _Feature objects.

  Args:
      data: --enable-features or --disable-features switch value string.
      is_disable: Parses --enable-features switch value string if True;
          --disable-features switch value string if False.
  """
  items = data.split(',')
  results = []
  for item in items:
    pair = item.split('<', 1)
    feature = pair[0]
    value = None
    if len(pair) > 1:
      value = pair[1]
    star = feature.startswith('*')
    if star:
      if is_disable:
        raise ValueError('--disable-features should not mark a feature with *')
      feature = feature[1:]
    results.append(_Feature(star, feature, value))
  return results


def _BuildFeaturesSwitchValue(features):
  """Generates --enable/--disable-features switch value string.

  This function does the opposite of _ParseFeatureListString().

  Args:
      features: A list of _Feature objects from which the switch value string
          is generated.
  """
  return ','.join('%s%s%s' % (
    '*' if feature.star else '',
    feature.key,
    '<%s' % feature.value if feature.value is not None else '',
  ) for feature in features)


def _SplitFeatures(features):
  """Splits a list of _Features objects into two lists.

  Note that either returned list could be empty, depending on the number of
  elements in the input list.
  """
  # Split a list of size N into two list: one of size middle, the other of size
  # N - middle. This works even when N is 0 or 1, resulting in empty list(s).
  middle = (len(features) + 1) // 2
  return features[:middle], features[middle:]


def ParseCommandLineSwitchesString(data):
  """Parses command line switches string into a dictionary.

  Format: { switch1:value1, switch2:value2, ..., switchN:valueN }.
  """
  switches = data.split('--')
  # The first one is always an empty string before the first '--'.
  switches = switches[1:]
  switch_data = {}
  for switch in switches:
    switch = switch.strip()
    fields = switch.split('=', 1) # Split by the first '='.
    if len(fields) != 2:
      raise ValueError('Wrong format, expected name=value, got %s' % switch)
    switch_name, switch_value = fields
    if switch_value[0] == '"' and switch_value[-1] == '"':
      switch_value = switch_value[1:-1]
    if (switch_name == _FORCE_FIELD_TRIALS_SWITCH_NAME and
        switch_value[-1] != '/'):
      # Older versions of Chrome do not include '/' in the end, but newer
      # versions do.
      # TODO(zmo): Verify if '/' is included in the end, older versions of
      # Chrome can still accept such switch.
      switch_value = switch_value + '/'
    switch_data[switch_name] = switch_value
  return switch_data


def ParseVariationsCmdFromString(input_string):
  """Parses commandline switches string into internal representation.

  Commandline switches string comes from chrome://version/?show-variations-cmd.
  Currently we parse the following four command line switches:
    --force-fieldtrials
    --force-fieldtrial-params
    --enable-features
    --disable-features
  """
  switch_data = ParseCommandLineSwitchesString(input_string)
  results = {}
  for switch_name, switch_value in switch_data.items():
    built_switch_value = None
    if switch_name == _FORCE_FIELD_TRIALS_SWITCH_NAME:
      results[switch_name] = _ParseForceFieldTrials(switch_value)
      built_switch_value = _BuildForceFieldTrialsSwitchValue(
          results[switch_name])
    elif switch_name == _DISABLE_FEATURES_SWITCH_NAME:
      results[switch_name] = _ParseFeatureListString(
          switch_value, is_disable=True)
      built_switch_value = _BuildFeaturesSwitchValue(results[switch_name])
    elif switch_name == _ENABLE_FEATURES_SWITCH_NAME:
      results[switch_name] = _ParseFeatureListString(
          switch_value, is_disable=False)
      built_switch_value = _BuildFeaturesSwitchValue(results[switch_name])
    elif switch_name == _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME:
      results[switch_name] = _ParseForceFieldTrialParams(switch_value)
      built_switch_value = _BuildForceFieldTrialParamsSwitchValue(
          results[switch_name])
    else:
      raise ValueError('Unexpected: --%s=%s', switch_name, switch_value)
    assert switch_value == built_switch_value
  if (_FORCE_FIELD_TRIALS_SWITCH_NAME in results and
      _FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME in results):
    _ValidateForceFieldTrialsAndParams(
        results[_FORCE_FIELD_TRIALS_SWITCH_NAME],
        results[_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME])
  return results


def ParseVariationsCmdFromFile(filename):
  """Parses commandline switches string into internal representation.

  Same as ParseVariationsCmdFromString(), except the commandline switches
  string comes from a file.
  """
  with open(filename, 'r') as f:
    data = f.read().replace('\n', ' ')
  return ParseVariationsCmdFromString(data)


def VariationsCmdToStrings(data):
  """Converts a dictionary of {switch_name:switch_value} to a list of strings.

  Each string is in the format of '--switch_name=switch_value'.

  Args:
      data: Input data dictionary. Keys are four commandline switches:
        'force-fieldtrials'
        'force-fieldtrial-params'
        'enable-features'
        'disable-features'

  Returns:
      A list of strings.
  """
  cmd_list = []
  force_field_trials = data[_FORCE_FIELD_TRIALS_SWITCH_NAME]
  if len(force_field_trials) > 0:
    force_field_trials_switch_value = _BuildForceFieldTrialsSwitchValue(
        force_field_trials)
    cmd_list.append('--%s="%s"' % (_FORCE_FIELD_TRIALS_SWITCH_NAME,
                                   force_field_trials_switch_value))
  force_field_trial_params = data[_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME]
  if len(force_field_trial_params) > 0:
    force_field_trial_params_switch_value = (
        _BuildForceFieldTrialParamsSwitchValue(force_field_trial_params))
    cmd_list.append('--%s="%s"' % (_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME,
                                   force_field_trial_params_switch_value))
  enable_features = data[_ENABLE_FEATURES_SWITCH_NAME]
  if len(enable_features) > 0:
    enable_features_switch_value = _BuildFeaturesSwitchValue(enable_features)
    cmd_list.append('--%s="%s"' % (_ENABLE_FEATURES_SWITCH_NAME,
                                   enable_features_switch_value))
  disable_features = data[_DISABLE_FEATURES_SWITCH_NAME]
  if len(disable_features) > 0:
    disable_features_switch_value = _BuildFeaturesSwitchValue(
        disable_features)
    cmd_list.append('--%s="%s"' % (_DISABLE_FEATURES_SWITCH_NAME,
                                   disable_features_switch_value))
  return cmd_list


def SplitVariationsCmd(results):
  """Splits internal representation of commandline switches into two.

  This function can be called recursively when bisecting a set of experiments
  until one is identified to be responsble for a certain browser behavior.

  The commandline switches come from chrome://version/?show-variations-cmd.
  """
  enable_features = results.get(_ENABLE_FEATURES_SWITCH_NAME, [])
  disable_features = results.get(_DISABLE_FEATURES_SWITCH_NAME, [])
  field_trials = results.get(_FORCE_FIELD_TRIALS_SWITCH_NAME, [])
  field_trial_params = results.get(_FORCE_FIELD_TRIAL_PARAMS_SWITCH_NAME, [])
  enable_features_splits = _SplitFeatures(enable_features)
  disable_features_splits = _SplitFeatures(disable_features)
  field_trials_splits = _SplitFieldTrials(field_trials, field_trial_params)
  splits = []
  for index in range(2):
    cmd_line = {}
    cmd_line.update(field_trials_splits[index])
    cmd_line[_ENABLE_FEATURES_SWITCH_NAME] = enable_features_splits[index]
    cmd_line[_DISABLE_FEATURES_SWITCH_NAME] = disable_features_splits[index]
    splits.append(cmd_line)
  return splits


def SplitVariationsCmdFromString(input_string):
  """Splits commandline switches.

  This function can be called recursively when bisecting a set of experiments
  until one is identified to be responsble for a certain browser behavior.

  Same as SplitVariationsCmd(), except data comes from a string rather than
  an internal representation.

  Args:
      input_string: Variations string to be split.

  Returns:
      If input can be split, returns a list of two strings, each is half of
      the input variations cmd; otherwise, returns a list of one string.
  """
  data = ParseVariationsCmdFromString(input_string)
  splits = SplitVariationsCmd(data)
  results = []
  for split in splits:
    cmd_list = VariationsCmdToStrings(split)
    if cmd_list:
      results.append(' '.join(cmd_list))
  return results


def SplitVariationsCmdFromFile(input_filename, output_dir=None):
  """Splits commandline switches.

  This function can be called recursively when bisecting a set of experiments
  until one is identified to be responsble for a certain browser behavior.

  Same as SplitVariationsCmd(), except data comes from a file rather than
  an internal representation.

  Args:
      input_filename: Variations file to be split.
      output_dir: Folder to output the split variations file(s). If None,
          output to the same folder as the input_filename. If the folder
          doesn't exist, it will be created.

  Returns:
      If input can be split, returns a list of two output filenames;
      otherwise, returns a list of one output filename.
  """
  with open(input_filename, 'r') as f:
    input_string = f.read().replace('\n', ' ')
  splits = SplitVariationsCmdFromString(input_string)
  dirname, filename = os.path.split(input_filename)
  basename, ext = os.path.splitext(filename)
  if output_dir is None:
    output_dir = dirname
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  split_filenames = []
  for index in range(len(splits)):
    output_filename = "%s_%d%s" % (basename, index + 1, ext)
    output_filename = os.path.join(output_dir, output_filename)
    with open(output_filename, 'w') as output_file:
      output_file.write(splits[index])
    split_filenames.append(output_filename)
  return split_filenames


def main():
  parser = optparse.OptionParser()
  parser.add_option("-f", "--file", dest="filename", metavar="FILE",
                    help="specify a file with variations cmd for processing.")
  parser.add_option("--output-dir", dest="output_dir",
                    help="specify a folder where output files are saved. "
                    "If not specified, it is the folder of the input file.")
  options, _ = parser.parse_args()
  if not options.filename:
    parser.error("Input file is not specificed")
  SplitVariationsCmdFromFile(options.filename, options.output_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())

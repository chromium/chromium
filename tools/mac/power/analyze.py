#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import pandas as pd
import plistlib


def GetDictionaryKeys(value, keys):
  """ Returns a dictionary containing `keys` from `value` if present. """
  return {key: value[key] for key in keys if key in value}


def FlattenDictionary(value, keys=[]):
  """ Returns a flattened version of the dictionary `value`, with nested keys
      combined as a.b.c. """
  result = {}
  if type(value) is dict:
    for key in value:
      result.update(FlattenDictionary(value[key], keys + [key]))
    return result
  else:
    key = '.'.join(keys)
    return {key: value}


def GetCoalition(coalitions_data, browser_identifier: str):
  """ Returns the coalition data whose name matches the identifier
     `browser_identifier`"""
  for coalition in coalitions_data:
    if coalition['name'] == browser_identifier:
      return coalition
  return None


def ReadPowerMetricsData(scenario_dir, browser_identifier: str):
  with open(os.path.join(scenario_dir, "powermetrics.plist"),
            "r") as plist_file:
    data = plist_file.read().split('\0')
    results = []
    for raw_sample in data:
      if raw_sample == "":
        continue
      parsed_sample = plistlib.loads(str.encode(raw_sample))
      out_sample = {'elapsed_ns': parsed_sample['elapsed_ns']}

      # Processing output of the 'tasks' sampler.
      coalition_keys = [
          "energy_impact", "gputime_ms", "diskio_byteswritten",
          "diskio_bytesread", "idle_wakeups", "intr_wakeups",
          "cputime_sample_ms_per_s", "cputime_ns"
      ]
      coalitions_data = parsed_sample['coalitions']
      if browser_identifier is not None:
        browser_coalition_data = GetCoalition(coalitions_data,
                                              browser_identifier)
        out_sample['browser'] = GetDictionaryKeys(browser_coalition_data,
                                                  coalition_keys)
      out_sample['all'] = GetDictionaryKeys(parsed_sample['all_tasks'],
                                            coalition_keys)

      # Add information for coalitions that could be of interest since they
      # might execute code on behalf of the browser.
      out_sample['window_server'] = GetDictionaryKeys(
          GetCoalition(coalitions_data, "com.apple.WindowServer"),
          coalition_keys)
      out_sample['kernel_coalition'] = GetDictionaryKeys(
          GetCoalition(coalitions_data, "kernel_coalition"), coalition_keys)

      # Processing output of the 'cpu_power' sampler.
      # Expected processor fields on Intel.
      out_sample['processor'] = GetDictionaryKeys(parsed_sample['processor'], [
          'freq_hz',
          'package_joules',
      ])
      # Expected processor fields on M1.
      out_sample['processor'].update(
          GetDictionaryKeys(parsed_sample['processor'], [
              'ane_energy',
              'dram_energy',
              'cpu_energy',
              'gpu_energy',
              'package_energy',
          ]))
      if 'clusters' in parsed_sample['processor']:
        for cluster in parsed_sample['processor']['clusters']:
          out_sample['processor'][cluster['name']] = GetDictionaryKeys(
              cluster, ['power', 'idle_ns', 'freq_hz'])
      results.append(FlattenDictionary(out_sample))
    return results


def NormalizeMicrosecondsSampleTime(sample_time: pd.Series):
  # Round sample time to .1s to aggregate similar rows across different sources.
  return (sample_time / 1000000.0).round(1)


def main():
  parser = argparse.ArgumentParser(
      description='Parses, aggregates and summarizes power results')
  parser.add_argument("--data_dir",
                      help="Directory containing benchmark data.",
                      required=True)
  parser.add_argument('--verbose',
                      action='store_true',
                      help='Print verbose output.')
  args = parser.parse_args()

  if args.verbose:
    log_level = logging.DEBUG
  else:
    log_level = logging.INFO
  logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

  for name in os.listdir(args.data_dir):
    subdir = os.path.join(args.data_dir, name)
    if not os.path.isdir(subdir):
      continue
    metadata_path = os.path.join(subdir, "metadata.json")
    if not os.path.isfile(metadata_path):
      continue
    with open(metadata_path, 'r') as metadata_file:
      metadata = json.load(metadata_file)
    logging.info(f'Found scenario {name}')
    if 'browser' in metadata:
      browser_identifier = metadata['browser']['identifier']
      logging.debug(f'Scenario running with {browser_identifier}')
    else:
      browser_identifier = None
    powermetrics_data = ReadPowerMetricsData(subdir, browser_identifier)
    powermetrics_dataframe = pd.DataFrame.from_records(powermetrics_data)
    # Add sample_time to powermetrics.
    powermetrics_dataframe["sample_time"] = NormalizeMicrosecondsSampleTime(
        powermetrics_dataframe['elapsed_ns'].cumsum() / 1000.0)
    powermetrics_dataframe.set_index('sample_time', inplace=True)

    with open(os.path.join(subdir, "power_sampler.json")) as power_file:
      power_data = json.load(power_file)
    power_dataframe = pd.DataFrame.from_records(
        power_data['data_rows'],
        columns=[key for key in power_data['column_labels']] + ["sample_time"])
    power_dataframe['sample_time'] = NormalizeMicrosecondsSampleTime(
        power_dataframe['sample_time'])
    power_dataframe.set_index('sample_time', inplace=True)

    # Join all data sources by sample_time.
    scenario_data = powermetrics_dataframe.join(power_dataframe, how='outer')

    summary_path = os.path.join(subdir, 'summary.csv')
    logging.info(f'Outputing results in {os.path.abspath(summary_path)}')
    scenario_data.to_csv(summary_path)


if __name__ == "__main__":
  main()

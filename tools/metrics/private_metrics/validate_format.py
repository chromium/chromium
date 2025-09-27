#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the DWA XML file is well-formatted."""

import sys
import argparse
from xml.dom import minidom

import private_metrics_validations


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('filepath', help="relative path to XML file")
  # The following optional flags are used by common/presubmit_util.py
  parser.add_argument('--presubmit', action="store_true")

  args = parser.parse_args()

  filepath = args.filepath

  if filepath.endswith('dwa.xml'):
    root_tag = 'dwa-configuration'
    validation = private_metrics_validations.DwaXmlValidation
  elif filepath.endswith('dkm.xml'):
    root_tag = 'dkm-configuration'
    validation = private_metrics_validations.DkmXmlValidation
  else:
    print(f'Unsupported file: {filepath}', file=sys.stderr)
    sys.exit(1)

  with open(filepath, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName(root_tag)
    validator = validation(config)

    owner_check_success, owner_check_errors = validator.checkEventsHaveOwners()
    metric_check_success, metric_check_errors = (
        validator.checkMetricTypeIsSpecified())

    results = dict()

    if (not owner_check_success or not metric_check_success):
      results['Errors'] = (owner_check_errors + metric_check_errors)

    if 'Errors' in results:
      return results


if __name__ == '__main__':
  sys.exit(main())

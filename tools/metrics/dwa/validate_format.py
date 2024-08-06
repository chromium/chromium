#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the DWA XML file is well-formatted."""

import os
import sys
import dwa_xml_validations

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

from xml.dom import minidom

_DWA_XML = path_util.GetInputFile('tools/metrics/dwa/dwa.xml')


def main():
  with open(_DWA_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('dwa-configuration')
    validator = dwa_xml_validations.DwaXmlValidation(config)

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

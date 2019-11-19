#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the UKM XML file is well-formatted."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

from xml_validations import UkmXmlValidation
from xml.dom import minidom

UKM_XML = path_util.GetInputFile('tools/metrics/ukm/ukm.xml')

IGNORE_METRIC_CHECK_WARNINGS = True


def main():
  with open(UKM_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('ukm-configuration')
    validator = UkmXmlValidation(config)

    ownerCheckSuccess, ownerCheckErrors = validator.checkEventsHaveOwners()
    metricCheckSuccess, metricCheckErrors, metricCheckWarnings = (
      validator.checkMetricTypeIsSpecified())
    aggregationCheckSuccess, aggregationCheckErrors = (
      validator.checkLocalMetricIsAggregated())

    results = dict();

    if (not ownerCheckSuccess or not metricCheckSuccess or
        not aggregationCheckSuccess):
      results['Errors'] = (ownerCheckErrors + metricCheckErrors +
                           aggregationCheckErrors)
    if metricCheckWarnings and not IGNORE_METRIC_CHECK_WARNINGS:
      results['Warnings'] = metricCheckWarnings

    if 'Warnings' in results or 'Errors' in results:
      return results


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the UKM XML file is well-formatted."""

import sys
from xml.dom import minidom

import setup_modules

import chromium_src.tools.metrics.common.path_util as path_util
from chromium_src.tools.metrics.ukm.xml_validations import UkmXmlValidation

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
    statisticCheckSuccess, statisticCheckErrors = (
        validator.checkStatisticsNonEmptyValid())

    results = dict();

    if (not ownerCheckSuccess or not metricCheckSuccess
        or not aggregationCheckSuccess or not statisticCheckSuccess):
      results['Errors'] = (ownerCheckErrors + metricCheckErrors +
                           aggregationCheckErrors + statisticCheckErrors)
    if metricCheckWarnings and not IGNORE_METRIC_CHECK_WARNINGS:
      results['Warnings'] = metricCheckWarnings

    if 'Warnings' in results or 'Errors' in results:
      return results


if __name__ == '__main__':
  sys.exit(main())

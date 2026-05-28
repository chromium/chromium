#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the UKM XML file is well-formatted."""

import sys
from xml.dom import minidom

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.path_util as path_util
from chromium_src.tools.metrics.ukm.xml_validations import UkmXmlValidation

UKM_XML = path_util.GetInputFile('tools/metrics/ukm/ukm.xml')

IGNORE_METRIC_CHECK_WARNINGS = True


def main():
  with open(UKM_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('ukm-configuration')
    validator = UkmXmlValidation(config)

    owner_check_success, owner_check_errors = (
        validator.check_events_have_owners())
    metric_check_success, metric_check_errors, metric_check_warnings = (
        validator.check_metric_type_is_specified())
    aggregation_check_success, aggregation_check_errors = (
        validator.check_local_metric_is_aggregated())
    statistic_check_success, statistic_check_errors = (
        validator.check_statistics_non_empty_valid())
    metric_name_check_success, metric_name_check_errors = (
        validator.check_metric_names())

    results = {}

    if (not owner_check_success or not metric_check_success
        or not aggregation_check_success or not statistic_check_success
        or not metric_name_check_success):
      results['Errors'] = (owner_check_errors + metric_check_errors +
                           aggregation_check_errors + statistic_check_errors +
                           metric_name_check_errors)
    if metric_check_warnings and not IGNORE_METRIC_CHECK_WARNINGS:
      results['Warnings'] = metric_check_warnings

    if 'Warnings' in results or 'Errors' in results:
      return results


if __name__ == '__main__':
  sys.exit(main())

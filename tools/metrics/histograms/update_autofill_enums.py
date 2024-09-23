#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Scans components/autofill/core/browser/field_types.h for FieldTypes
and updates histograms that are calculated from this enum.
"""

import optparse
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import update_histogram_enum

FIELD_TYPES_PATH = 'components/autofill/core/browser/field_types.h'
FIELD_PREDICTION_GROUPS_PATH = \
    'components/autofill/core/browser/metrics/autofill_metrics.cc'


def ReadEnum(filename, first_line, last_line_exclusive):
  """Extracts an enum from a file.

  Args:
    filename: name of file to read.
    first_line: prefix of a line where extracting values starts. Leading
      whitespaces are ignored.
    last_line_exclusive: prefix of a line before which extraction stops.
      This means that the line matching this parameter is not considered
      a valid enum entry that is returned. Leading whitespaces are ignored.

  Returns:
    Dictionary from enum ids (integers) to names (strings).
  """

  # Read the enum FieldType as a list of lines.
  before_enum = True
  content = []
  with open(path_util.GetInputFile(filename)) as f:
    for line in f.readlines():
      stripped_line = line.strip()
      # Search for beginning of enum.
      if before_enum:
        if stripped_line.startswith(first_line):
          before_enum = False
        continue
      # Terminate at end of enum.
      if stripped_line.startswith(last_line_exclusive):
        break
      content.append(stripped_line)

  ENUM_REGEX = re.compile(
      r"""^\s*?(\w+)\s+=         # capture the enum name
          \s+(\d+)(?:,.*)?$  # capture the id
          """, re.VERBOSE)

  enums = {}
  for line in content:
    enum_match = ENUM_REGEX.search(line)
    if enum_match:
      enum_name = enum_match.group(1)
      enum_id = int(enum_match.group(2))
      enums[enum_id] = enum_name

  return enums


def ReadFieldTypes(filename):
  return ReadEnum(filename, 'enum FieldType {', 'MAX_VALID_FIELD_TYPE =')


def ReadFieldPredictionGroups(filename):
  result = ReadEnum(filename, 'enum FieldTypeGroupForMetrics {',
                    'NUM_FIELD_TYPE_GROUPS_FOR_METRICS')
  # Strip the GROUP_ prefix because it only adds clutter.
  return {k: v.replace('GROUP_', '') for k, v in result.items()}


def GenerateAutofilledFieldUserEditingStatusByFieldType(server_field_types):
  result = {}
  for enum_id, enum_name in server_field_types.items():
    result[16 * enum_id + 0] = f'{enum_name}: edited'
    result[16 * enum_id + 1] = f'{enum_name}: accepted'
  return result


def GenerateAutofillPredictionsComparisonResult(server_field_types):
  result = {}
  result[0] = 'None'
  for id, name in server_field_types.items():
    result[6 * id + 1] = f'{name} - Predictions equal - Value agrees'
    result[6 * id + 2] = f'{name} - Predictions equal - Value disagrees'
    result[6 * id + 3] = \
        f'{name} - Predictions different - Value agrees with old prediction'
    result[6 * id + 4] = \
        f'{name} - Predictions different - Value agrees with new prediction'
    result[6 * id + 5] = \
        f'{name} - Predictions different - Value agrees with neither prediction'
    result[6 * id + 6] = \
        f'{name} - Predictions different - Value agrees with both predictions'
  return result


def GenerateAutofillFieldPredictionQualityByFieldType():
  groups = ReadFieldPredictionGroups(FIELD_PREDICTION_GROUPS_PATH)
  result = {}
  for enum_id, enum_name in groups.items():
    result[256 * enum_id + 0] = f'{enum_name}: True Positive'
    result[256 * enum_id + 1] = f'{enum_name}: True Negative (Ambiguous)'
    result[256 * enum_id + 2] = f'{enum_name}: True Negative (Unknown)'
    result[256 * enum_id + 3] = f'{enum_name}: True Negative (Empty)'
    result[256 * enum_id + 4] = f'{enum_name}: False Positive (Mismatch)'
    result[256 * enum_id + 5] = f'{enum_name}: False Positive (Ambiguous)'
    result[256 * enum_id + 6] = f'{enum_name}: False Positive (Unknown)'
    result[256 * enum_id + 7] = f'{enum_name}: False Positive (Empty)'
    result[256 * enum_id + 8] = f'{enum_name}: False Negative (Mismatch)'
    result[256 * enum_id + 9] = f'{enum_name}: False Negative (Unknown)'
  return result


def GenerateAutofillPreFilledFieldStatusByFieldType(field_types):
  result = {}
  for enum_id, enum_name in field_types.items():
    result[16 * enum_id + 0] = f'{enum_name}: Pre-filled on page load'
    result[16 * enum_id + 1] = f'{enum_name}: Empty on page load'
  return result


def GenerateAutofillDataUtilizationByFieldType(field_types):
  result = {}
  for enum_id, enum_name in field_types.items():
    result[64 * enum_id +
           0] = f'{enum_name}: Not autofilled or autofilled value edited'
    result[64 * enum_id + 1] = f'{enum_name}: Autofilled value accepted'
  return result


if __name__ == '__main__':
  server_field_types = ReadFieldTypes(FIELD_TYPES_PATH)

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofillFieldType', server_field_types, FIELD_TYPES_PATH,
      os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofilledFieldUserEditingStatusByFieldType',
      GenerateAutofilledFieldUserEditingStatusByFieldType(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofillPredictionsComparisonResult',
      GenerateAutofillPredictionsComparisonResult(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofillFieldPredictionQualityByFieldType',
      GenerateAutofillFieldPredictionQualityByFieldType(),
      FIELD_PREDICTION_GROUPS_PATH, os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofillPreFilledFieldStatusByFieldType',
      GenerateAutofillPreFilledFieldStatusByFieldType(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/metadata/autofill/enums.xml',
      'AutofillDataUtilizationByFieldType',
      GenerateAutofillDataUtilizationByFieldType(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

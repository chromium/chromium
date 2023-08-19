#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Scans components/autofill/core/browser/field_types.h for ServerFieldTypes
and updates histograms that are calculated form this enum.
"""

import optparse
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import update_histogram_enum

FIELD_TYPES_PATH = 'components/autofill/core/browser/field_types.h'


def ReadServerFieldTypes(filename):
  # Read the enum ServerFieldType as a list of lines
  before_enum = True
  content = []
  with open(path_util.GetInputFile(filename)) as f:
    for line in f.readlines():
      # Search for beginning of enum.
      if before_enum:
        if line.startswith('enum ServerFieldType {'):
          before_enum = False
        continue
      # Terminate at end of enum.
      if line.startswith('  MAX_VALID_FIELD_TYPE ='):
        break
      content.append(line)

  ENUM_REGEX = re.compile(
      r"""^\s+(\w+)\s+=   # capture the enum name
          \s+(\d+),?$     # capture the id
          """, re.VERBOSE)

  enums = {}
  for line in content:
    enum_match = ENUM_REGEX.search(line)
    if enum_match:
      enum_name = enum_match.group(1)
      enum_id = int(enum_match.group(2))
      enums[enum_id] = enum_name

  return enums


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


if __name__ == '__main__':
  server_field_types = ReadServerFieldTypes(FIELD_TYPES_PATH)

  update_histogram_enum.UpdateHistogramFromDict('AutofillServerFieldType',
                                                server_field_types,
                                                FIELD_TYPES_PATH,
                                                os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'AutofilledFieldUserEditingStatusByFieldType',
      GenerateAutofilledFieldUserEditingStatusByFieldType(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

  update_histogram_enum.UpdateHistogramFromDict(
      'AutofillPredictionsComparisonResult',
      GenerateAutofillPredictionsComparisonResult(server_field_types),
      FIELD_TYPES_PATH, os.path.basename(__file__))

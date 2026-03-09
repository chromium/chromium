# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for Private Metrics XML configuration.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import sys

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.
# TODO(crbug.com/488351821): Consider using subprocesses to run actual
#                            test as recommended by presubmit docs:
# https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
sys.path.append('.')
import setup_modules

sys.path.remove('.')

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util

DWA_XML = 'dwa.xml'
DKM_XML = 'dkm.xml'


def CheckChangeOnUpload(input_api, output_api):
  result = []
  result.extend(presubmit_util.CheckChange(DWA_XML, input_api, output_api))
  result.extend(presubmit_util.CheckChange(DKM_XML, input_api, output_api))
  return result


def CheckChangeOnCommit(input_api, output_api):
  result = []
  result.extend(presubmit_util.CheckChange(DWA_XML, input_api, output_api))
  result.extend(presubmit_util.CheckChange(DKM_XML, input_api, output_api))
  return result

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
# The helper below runs in-process because it needs input_api's view of the
# change to decide whether dwa.xml has to be checked.
sys.path.append('.')
import setup_modules  # pylint: disable=unused-import

sys.path.remove('.')

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util

DWA_XML = 'dwa.xml'


def CheckChangeOnUpload(input_api, output_api):
  result = []
  result.extend(presubmit_util.CheckChange(DWA_XML, input_api, output_api))
  return result


def CheckChangeOnCommit(input_api, output_api):
  result = []
  result.extend(presubmit_util.CheckChange(DWA_XML, input_api, output_api))
  return result

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ukm.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import sys

# PRESUBMIT infrastructure doesn't guarantee that the cwd() will be on
# path requiring manual path manipulation to call setup_modules.
# The helper below runs in-process because it needs input_api's view of the
# change to decide whether ukm.xml has to be checked.
sys.path.append('.')
import setup_modules  # pylint: disable=unused-import

sys.path.remove('.')

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util

UKM_XML = 'ukm.xml'


def CheckChangeOnUpload(input_api, output_api):
  return presubmit_util.CheckChange(UKM_XML, input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return presubmit_util.CheckChange(UKM_XML, input_api, output_api)

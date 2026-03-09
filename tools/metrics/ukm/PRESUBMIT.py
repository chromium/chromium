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
# TODO(crbug.com/488351821): Consider using subprocesses to run actual
#                            test as recommended by presubmit docs:
# https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
sys.path.append('.')
import setup_modules

sys.path.remove('.')

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util

UKM_XML = 'ukm.xml'

def CheckChangeOnUpload(input_api, output_api):
  return presubmit_util.CheckChange(UKM_XML, input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return presubmit_util.CheckChange(UKM_XML, input_api, output_api)

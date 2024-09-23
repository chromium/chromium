# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for dwa.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import os
import sys

sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath('__file__')), '..', 'common'))
import presubmit_util

_DWA_XML = 'dwa.xml'


def CheckChangeOnUpload(input_api, output_api):
  return presubmit_util.CheckChange(_DWA_XML, input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return presubmit_util.CheckChange(_DWA_XML, input_api, output_api)

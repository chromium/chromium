# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit tests for weblayer public API."""

import glob
import logging
import os
import shutil
import subprocess
import sys
import tempfile

USE_PYTHON3 = True

_WEBLAYER_PUBLIC_MANIFEST_PATH=os.path.join("weblayer", "public", "java",
                                            "AndroidManifest.xml")

_MANIFEST_CHANGE_STRING = """You are changing the WebLayer public manifest.
Did you validate this change with WebLayer's internal clients?
If not, you must do so before landing it!"""

def CheckChangeOnUpload(input_api, output_api):
  for f in input_api.AffectedFiles():
    if _WEBLAYER_PUBLIC_MANIFEST_PATH in f.AbsoluteLocalPath():
      return [output_api.PresubmitPromptWarning(
        _MANIFEST_CHANGE_STRING)]

  return []

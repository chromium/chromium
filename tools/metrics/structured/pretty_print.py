#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.path_util as path_util

import chromium_src.tools.metrics.structured.sync.model as model
import chromium_src.tools.metrics.common.presubmit_util as presubmit_util

def main():
  """Pretty-prints the structured metrics in structured.xml file."""
  structured_dir = path_util.METRICS_TOOLS_PATH / 'structured'
  xml = str(structured_dir / 'sync' / 'structured.xml')
  old_xml = str(structured_dir / 'sync' / 'structured.old.xml')

  presubmit_util.DoPresubmitMain(xml, old_xml,
                                 lambda x: repr(model.Model(x, 'chrome')))


if __name__ == '__main__':
  main()

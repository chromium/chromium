#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util
import chromium_src.tools.metrics.common.utf8_encoding as utf8_encoding
import chromium_src.tools.metrics.ukm.ukm_model as ukm_model

def main():
  """Pretty-prints the Chrome UKM events in ukm.xml file."""
  utf8_encoding.setup_stdout_and_stderr_utf8_encoding()

  presubmit_util.DoPresubmitMain('ukm.xml', 'ukm.old.xml',
                                 ukm_model.prettify_xml_and_trim_obsolete)


if __name__ == '__main__':
  main()

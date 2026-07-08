#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.presubmit_util as presubmit_util
import chromium_src.tools.metrics.common.utf8_encoding as utf8_encoding
import chromium_src.tools.metrics.actions.extract_actions as extract_actions


def main():
  """Pretty-prints the User Actions in actions.xml file."""
  utf8_encoding.setup_stdout_and_stderr_utf8_encoding()

  presubmit_util.DoPresubmitMain(
      'actions.xml', 'actions.old.xml',
      lambda file_content: extract_actions.UpdateXml(
          file_content, extract_actions._GeneratedActions()))


if __name__ == '__main__':
  main()

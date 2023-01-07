#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import extract_actions
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util

def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'actions.xml', 'actions.old.xml',
                                 extract_actions.UpdateXml)


if '__main__' == __name__:
  sys.exit(main(sys.argv))

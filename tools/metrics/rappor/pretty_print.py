#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import rappor_model
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util

def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'rappor.xml', 'rappor.old.xml',
                                 'pretty_print.py', rappor_model.UpdateXML)


if '__main__' == __name__:
  sys.exit(main(sys.argv))

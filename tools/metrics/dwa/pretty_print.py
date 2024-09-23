#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import dwa_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util


def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'dwa.xml', 'dwa.old.xml',
                                 dwa_model.PrettifyXML)


if '__main__' == __name__:
  sys.exit(main(sys.argv))

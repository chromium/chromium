#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import ukm_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util



def main():
  """Pretty-prints the Chrome UKM events in ukm.xml file."""
  presubmit_util.DoPresubmitMain('ukm.xml',
                                 'ukm.old.xml',
                                 ukm_model.PrettifyXmlAndTrimObsolete,
                                 description=main.__doc__)


if __name__ == '__main__':
  main()

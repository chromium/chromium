#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import extract_actions
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util


def main():
  """Pretty-prints the User Actions in actions.xml file."""
  presubmit_util.DoPresubmitMain('actions.xml',
                                 'actions.old.xml',
                                 extract_actions.UpdateXml,
                                 description=main.__doc__)


if __name__ == '__main__':
  main()

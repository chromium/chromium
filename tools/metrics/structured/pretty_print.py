#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from sync import model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util


def main():
  """Pretty-prints the structured metrics in structured.xml file."""
  dirname = os.path.dirname(os.path.realpath(__file__))
  xml = dirname + '/sync/structured.xml'
  old_xml = dirname + '/sync/structured.old.xml'

  presubmit_util.DoPresubmitMain(xml,
                                 old_xml,
                                 lambda x: repr(model.Model(x, 'chrome')),
                                 description=main.__doc__)


if __name__ == '__main__':
  main()

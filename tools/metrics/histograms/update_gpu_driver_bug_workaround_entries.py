#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates GpuDriverBugWorkaroundEntry enum in histograms.xml file with values
 read from gpu_driver_bug_list.json.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import os.path
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util
import json

import update_histogram_enum


GPU_DRIVER_BUG_WORKAROUND_PATH = 'gpu/config/gpu_driver_bug_list.json'


def ReadGpuDriverBugEntries(filename):
  """Reads in the gpu driver bug list, returning a dictionary mapping
  workaround ids to descriptions.
  """
  # Read the file as a list of lines
  with open(path_util.GetInputFile(filename)) as f:
    json_data = json.load(f)

  entries = {}
  entries[0] = '0: Recorded once every time this histogram is updated.'
  for entry in json_data["entries"]:
    entries[entry["id"]] = "%d: %s" % (entry["id"], entry["description"])
  return entries


def main():
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  update_histogram_enum.UpdateHistogramFromDict(
      'GpuDriverBugWorkaroundEntry',
      ReadGpuDriverBugEntries(GPU_DRIVER_BUG_WORKAROUND_PATH),
      GPU_DRIVER_BUG_WORKAROUND_PATH,
      os.path.basename(__file__))


if __name__ == '__main__':
  main()

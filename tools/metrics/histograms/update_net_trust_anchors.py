#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates NetTrustAnchors enum in histograms.xml file with values read
 from net/data/ssl/root_stores/root_stores.json.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import json
import os.path
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import update_histogram_enum

NET_ROOT_CERTS_PATH = 'net/data/ssl/root_stores/root_stores.json'

def main():
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  with open(path_util.GetInputFile(NET_ROOT_CERTS_PATH)) as f:
    root_stores = json.load(f)

  spki_enum = {}
  spki_enum[0] = 'Unknown or locally-installed trust anchor'
  for spki, spki_data in sorted(root_stores['spkis'].items()):
    spki_enum[int(spki_data['id'])] = spki

  update_histogram_enum.UpdateHistogramFromDict(
    'NetTrustAnchors', spki_enum, NET_ROOT_CERTS_PATH,
    os.path.basename(__file__))

if __name__ == '__main__':
  main()

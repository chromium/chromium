#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to alphabetically order message ids in
  .grd files of the /ios folder.

Sample use:

python3 tools/grd/ios_order_grd_message_ids.py
"""

import os
import xml.etree.ElementTree as etree
import sys
from pathlib import Path

SRC_DIR = Path(__file__).parents[2]
IOS_GRD_FILES = [
    os.path.join(SRC_DIR, "ios/chrome/app/strings/ios_strings.grd"),
    os.path.join(SRC_DIR, "ios/chrome/app/strings/ios_chromium_strings.grd"),
    os.path.join(SRC_DIR,
                 "ios/chrome/app/strings/ios_google_chrome_strings.grd")
]


def order_message_ids():
  for file in IOS_GRD_FILES:
    parser = etree.XMLParser(target=etree.TreeBuilder(insert_comments=True))
    tree = etree.parse(file, parser)
    root = tree.getroot()

    for node in root.findall(".//messages"):
      node[:] = sorted(node, key=lambda node: node.get("name"))

    tree.write(file, encoding='UTF-8', xml_declaration=True)


def main():
  order_message_ids()


if __name__ == '__main__':
  sys.exit(main())

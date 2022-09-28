#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

samples_pages = [
  'xr-barebones.html',
  'magic-window.html',
  'teleportation.html',
  'gamepad.html'
]

other_pages = [
  'attribution.html',
  'favicon-32x32.png',
  'favicon-96x96.png',
  'favicon.ico',
  'LICENSE.md'
]

copy_folders = [
  'css',
  'js'
]

def make_ot_samples_folder(source, dest):
  os.mkdir(dest)
  for f in samples_pages:
    shutil.copy(os.path.join(source, f), dest)
  for f in other_pages:
    shutil.copy(os.path.join(source, f), dest)
  for f in copy_folders:
    shutil.copytree(os.path.join(source, f), os.path.join(dest, f))
  shutil.copy(
    os.path.join(source, 'index.published.html'),
    os.path.join(dest, 'index.html'))
  shutil.make_archive('source', 'zip', dest)
  shutil.move('source.zip', dest)

  # media folder won't be included in the zip file or uploaded in any way as
  # part of this process
  shutil.copytree(os.path.join(source, 'media'), os.path.join(dest, 'media'))

def main():
  make_ot_samples_folder(sys.argv[1], sys.argv[2])

if __name__ == '__main__':
  main()

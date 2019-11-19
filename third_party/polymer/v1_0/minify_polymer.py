# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Minifies Polymer 2, since it does not come already minified from bower
(unlike Polymer 1).
"""

import os
import sys
import shutil
import tempfile

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

sys.path.append(_HERE_PATH)
import extract_inline_scripts

def main():
  polymer_dir = os.path.join(_HERE_PATH, 'components-chromium', 'polymer2')
  # Final HTML bundle.
  polymer_html = os.path.join(polymer_dir, 'polymer.html')
  # Final JS bundle.
  polymer_js = os.path.join(polymer_dir, 'polymer-extracted.js')

  # Move the entire checkout to a temp location.
  tmp_dir = os.path.join(_HERE_PATH, 'components-chromium', 'polymer2temp')
  if os.path.exists(tmp_dir):
    shutil.rmtree(tmp_dir)
  shutil.move(polymer_dir, tmp_dir)

  tmp_out_dir = os.path.join(tmp_dir, 'out')
  os.makedirs(tmp_out_dir)

  try:
    # Combine everything to a single HTML bundle file.
    node.RunNode([
        node_modules.PathToBundler(),
        '--strip-comments',
        '--inline-scripts',
        '--inline-css',
        '--out-file', os.path.join(tmp_out_dir, 'polymer.html'),
        os.path.join(tmp_dir, 'polymer.html'),
    ])

    # Extract the JS to a separate file named polymer-extracted.js.
    extract_inline_scripts.ExtractFrom(
            os.path.join(tmp_out_dir, 'polymer.html'))

    # Minify the JS bundle.
    extracted_js = os.path.join(tmp_out_dir, 'polymer-extracted.js')
    node.RunNode([
        node_modules.PathToUglify(), extracted_js,
        '--comments', '"/Copyright|license|LICENSE/"',
        '--output', extracted_js])

    # Copy generated bundled JS/HTML files back to the original location.
    os.makedirs(polymer_dir)
    shutil.move(os.path.join(tmp_out_dir, 'polymer.html'), polymer_html)
    shutil.move(extracted_js, polymer_js)

    # Copy a few more files.
    shutil.move(os.path.join(tmp_dir, 'bower.json'), polymer_dir)
    shutil.move(os.path.join(tmp_dir, 'LICENSE.txt'), polymer_dir)
  finally:
    # Delete component-chromium/shadycss since it ends up in the bundle.
    shutil.rmtree(os.path.join(_HERE_PATH, 'components-chromium', 'shadycss'))
    shutil.rmtree(tmp_dir)

if __name__ == '__main__':
  main()

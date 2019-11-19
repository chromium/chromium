# Copyrigh 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Minifies Polymer 3, since it does not come already minified from NPM."""

import os
import shutil
import sys
import tempfile

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules


def main():
  polymer_dir = os.path.join(_HERE_PATH, 'components-chromium', 'polymer')

  # Copy the top-level Polymer file that holds all dependencies. This file is
  # not distributed via NPM, it only exists within third_party/polymer
  # repository.
  shutil.copy(
      os.path.join(polymer_dir, '..', '..', 'polymer.js'), polymer_dir);

  # Move the entire checkout to a temp location.
  tmp_dir = os.path.join(_HERE_PATH, 'components-chromium', 'polymer_temp')
  if os.path.exists(tmp_dir):
    shutil.rmtree(tmp_dir)
  shutil.move(polymer_dir, tmp_dir)

  tmp_out_dir = os.path.join(tmp_dir, 'out')
  os.makedirs(tmp_out_dir)

  try:
    # Combine everything to a single JS bundle file.
    bundled_js = os.path.join(tmp_out_dir, 'polymer_bundled.js')
    path_to_rollup = os.path.join('node_modules', 'rollup', 'bin', 'rollup');

    node.RunNode([
        path_to_rollup,
        # See https://github.com/rollup/rollup/issues/1955
        '--silent',
        '--format', 'esm',
        '--input', os.path.join(tmp_dir, 'polymer.js'),
        '--file', bundled_js,
    ])

    # Minify the JS bundle.
    minified_js = os.path.join(tmp_out_dir, 'polymer_bundled.min.js')
    node.RunNode([
        node_modules.PathToUglify(), bundled_js,
        # TODO(dpapad): Figure out a way to deduplicate LICENSE headers.
        #'--comments', '"/Copyright|license|LICENSE/"',
        '--output', minified_js])

    # Copy generated JS bundle back to the original location.
    os.makedirs(polymer_dir)
    shutil.move(minified_js, polymer_dir)

    # Copy LICENSE file.
    shutil.copy(os.path.join(tmp_dir, 'LICENSE.txt'), polymer_dir)

    # Copy files needed for type checking.
    # - |bundled_js| is the JS bundle with JS type annotations.
    # - various externs files
    shutil.copy(bundled_js, polymer_dir)
    externs_to_copy = [
      os.path.join(tmp_dir, 'externs', 'closure-types.js'),
      os.path.join(tmp_dir, 'externs', 'polymer-dom-api-externs.js'),
      os.path.join(tmp_dir, 'externs', 'polymer-externs.js'),
      os.path.join(tmp_dir, 'externs', 'webcomponents-externs.js'),
      os.path.join(
          polymer_dir, '..', 'shadycss', 'externs', 'shadycss-externs.js'),
    ]
    externs_dir = os.path.join(polymer_dir, 'externs')
    os.makedirs(externs_dir)
    for extern in externs_to_copy:
      shutil.copy(extern, externs_dir)

  finally:
    # Delete component-chromium/shadycss since it ends up in the bundle.
    shutil.rmtree(os.path.join(_HERE_PATH, 'components-chromium', 'shadycss'))
    shutil.rmtree(tmp_dir)


if __name__ == '__main__':
  main()

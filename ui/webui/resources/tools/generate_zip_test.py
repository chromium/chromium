#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import generate_zip
import os
import shutil
import tempfile
import unittest
import zipfile

_HERE_DIR = os.path.dirname(__file__)


class GenerateZipTest(unittest.TestCase):
  def setUp(self):
    self._temp_dir = tempfile.mkdtemp(dir=_HERE_DIR)

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def _write_file(self, rel_path, content):
    full_path = os.path.join(self._temp_dir, rel_path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, 'w', encoding='utf-8') as f:
      f.write(content)
    return full_path

  def testGenerateZip(self):
    gen_dir = os.path.join(self._temp_dir, 'gen')
    src_dir = os.path.join(self._temp_dir, 'src')

    self._write_file('gen/sub/app.js', 'console.log("hello");')
    self._write_file('src/manifest.json', '{"name": "ext"}')
    self._write_file(
      'gen/manifest.grdp',
      """<?xml version="1.0" encoding="UTF-8"?>
<grit-part>
  <include name="IDR_EXT_MANIFEST"
      file="${root_src_dir}/manifest.json"
      resource_path="my_prefix/manifest.json"
      use_base_dir="false" type="BINDATA" />
</grit-part>
""",
    )
    in_grd = self._write_file(
      'gen/resources.grd',
      """<?xml version="1.0" encoding="UTF-8"?>
<grit latest_public_release="0" current_release="1">
  <release seq="1">
    <includes>
      <part file="manifest.grdp" />
      <include name="IDR_EXT_APP_JS"
          file="${root_gen_dir}/sub/app.js"
          resource_path="my_prefix/sub/app.js"
          use_base_dir="false" type="BINDATA" />
    </includes>
  </release>
</grit>
""",
    )

    out_zip = os.path.join(self._temp_dir, 'resources.zip')

    generate_zip.main(
      [
        '--in-grd',
        in_grd,
        '--out-zip',
        out_zip,
        '--root-gen-dir',
        gen_dir,
        '--root-src-dir',
        src_dir,
        '--grd-resource-path-prefix',
        'my_prefix',
      ]
    )

    self.assertTrue(os.path.isfile(out_zip))
    with zipfile.ZipFile(out_zip, 'r') as zf:
      self.assertEqual(zf.namelist(), ['manifest.json', 'sub/app.js'])
      self.assertEqual(
        zf.read('manifest.json').decode('utf-8'), '{"name": "ext"}'
      )
      self.assertEqual(
        zf.read('sub/app.js').decode('utf-8'), 'console.log("hello");'
      )


if __name__ == '__main__':
  unittest.main()

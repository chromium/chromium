#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import subprocess
import tempfile
import unittest
import zipfile

import path_util
import zip_util


def _FindZipAlign():
  # SDK does not exist on presubmit CQ bot.
  candidates = glob.glob(
    path_util.FromToolsSrcRootRelative(
        'third_party/android_sdk/public/build-tools/*/zipalign'))
  # Any version will do.
  return candidates[0] if candidates else None


class ZipUtilTest(unittest.TestCase):
  def testReadZipInfoExtraFieldLength(self):
    zipalign = _FindZipAlign()
    if not zipalign:
      return

    with tempfile.NamedTemporaryFile() as f:
      with zipfile.ZipFile(f, 'w') as z:
        z.writestr('a', 'a')
        z.writestr('b', 'bb')
        z.writestr('c', 'ccc')
      f.flush()
      with tempfile.NamedTemporaryFile() as f2:
        subprocess.run([zipalign, '-f', '4', f.name, f2.name], check=True)
        with zipfile.ZipFile(f2) as z:
          alignments = [
              zip_util.ReadZipInfoExtraFieldLength(z, i) for i in z.infolist()
          ]

    self.assertEqual([1, 0, 3], alignments)

  def testMeasureApkSignatureBlock(self):
    with tempfile.NamedTemporaryFile() as f:
      with zipfile.ZipFile(f, 'w') as z:
        z.writestr('a', 'a')
      f.flush()
      with zipfile.ZipFile(f) as z:
        # It's non-trivial to sign an apk, so at least make sure the logic
        # reports 0 for an unsigned .zip.
        self.assertEqual(0, zip_util.MeasureApkSignatureBlock(z))


if __name__ == '__main__':
  unittest.main()

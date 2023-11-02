#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
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
      path_util.FromToolsSrcRoot(
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

    # Some versions of zipalign add 4 bytes when none are needed :/.
    # https://android-review.googlesource.com/c/platform/build/+/1467998/9#message-e05d1da10ab2189ddd662fcb14a8006114f8a206
    alignments = [x % 4 for x in alignments]
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

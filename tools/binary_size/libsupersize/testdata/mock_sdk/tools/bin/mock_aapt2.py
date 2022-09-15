# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import zipfile


def main():
  apk_path = sys.argv[-1]
  assert os.path.exists(apk_path), 'Apk does not exist: {}'.format(apk_path)
  with zipfile.ZipFile(apk_path) as z:
    try:
      # The AndroidManifest.xml file will have the aapt2 output, and not XML.
      with z.open('AndroidManifest.xml') as f:
        sys.stdout.write(f.read().decode('utf8'))
    except KeyError:
      pass


if __name__ == '__main__':
  main()

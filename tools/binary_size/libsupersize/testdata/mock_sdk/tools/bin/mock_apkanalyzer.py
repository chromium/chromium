# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys
import zipfile


_SCRIPT_DIR = os.path.dirname(__file__)
_OUTPUT_FILE = os.path.join(_SCRIPT_DIR, 'apkanalyzer.output')


def main():
  # Without a proguard mapping file, the last argument is the apk_path.
  apk_path = sys.argv[-1]
  assert os.path.exists(apk_path), 'Apk does not exist: {}'.format(apk_path)
  with zipfile.ZipFile(apk_path) as z:
    if any(n.endswith('.dex') for n in z.namelist()):
      with open(_OUTPUT_FILE, 'r') as f:
        sys.stdout.write(f.read())
    else:
      sys.stdout.write('P r 0   0       0       <TOTAL>')


if __name__ == '__main__':
  main()

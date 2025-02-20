# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Like /common/slow.py except with text/html content-type so that it won't
# trigger strange parts of the <iframe> navigate algorithm. Also include
# Supports-Loading-Mode: fenced-frame to allow loading in fenced frame.

import time


def main(request, response):
    delay = float(request.GET.first(b"delay", 2000)) / 1000
    time.sleep(delay)
    return 200, [("Content-Type", "text/html"),
                 ("Supports-Loading-Mode", "fenced-frame")], b''

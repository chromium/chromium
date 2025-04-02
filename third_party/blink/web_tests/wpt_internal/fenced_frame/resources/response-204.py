# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def main(request, response):
    return 204, [("Content-Type", "text/html"),
                 ("Supports-Loading-Mode", "fenced-frame")], b"No content"

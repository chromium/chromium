# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Serves a valid worklet script only when the `Sec-GPC` header is present on
# the worklet script load request itself. When the header is missing, the
# response is a 404.


def main(request, _):
    has_gpc_header = request.headers.get(b"Sec-GPC") == b"1"

    response_headers = [(b"Content-Type", b"text/javascript")]

    if has_gpc_header:
        return (200, response_headers, b"""
// Do nothing.
""")
    return (404, response_headers, b"")

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Serves a worker script that reports whether the `Sec-GPC` header was present
# on the worker script load request itself.


def main(request, response):
    has_gpc_header = request.headers.get(b"Sec-GPC") == b"1"
    gpc_literal = b"true" if has_gpc_header else b"false"

    response.headers.set(b"Content-Type", b"application/javascript")

    # Supports both dedicated workers (onmessage) and shared workers (onconnect).
    return b"""
const kScriptLoadHadGpcHeader = %s;

self.onmessage = () => {
  self.postMessage(kScriptLoadHadGpcHeader);
};

self.onconnect = (event) => {
  const port = event.ports[0];
  port.onmessage = () => {
    port.postMessage(kScriptLoadHadGpcHeader);
  };
};
""" % gpc_literal

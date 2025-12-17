// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ResponseHeader {
  DOMString value;
};

// Stub namespace for manifest keys relating to the cross origin isolation
// response headers.
[Namespace=crossOriginIsolation]
partial dictionary ExtensionManifest {
  ResponseHeader cross_origin_embedder_policy;
  ResponseHeader cross_origin_opener_policy;
};

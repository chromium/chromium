// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This script is injected into a page to extract metadata.
 * It assumes Readability.js and metadata_processor.js have been injected.
 */
(() => {
  return window.ReadabilityExtension.extractMetadata(document);
})();

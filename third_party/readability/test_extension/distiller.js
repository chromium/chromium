// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This script is injected into a page to run the Readability parser.
 * It assumes Readability.js, article_renderer.js, and article_processor.js
 * have been injected.
 */
(async () => {
  return await window.ReadabilityExtension.processAndRenderArticle(document);
})();

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  // Create a shared namespace.
  window.ReadabilityExtension = window.ReadabilityExtension || {};

  /**
   * A centralized function to invoke Readability.js and return only the
   * metadata fields from the parsed article.
   *
   * @param {Document} doc The document to parse.
   * @return {object|null} The article object containing only metadata, or null
   *     if parsing fails.
   */
  window.ReadabilityExtension.extractMetadata = function(doc) {
    try {
      const docClone = doc.cloneNode(true);
      const article = new Readability(docClone).parse();

      if (article) {
        // We only want the metadata, not the content.
        delete article.content;
        delete article.textContent;
      }

      return article;
    } catch (e) {
      console.error('Error during metadata extraction: ' + e);
      if (e.stack) {
        console.error(e.stack);
      }
      return null;
    }
  }
})();

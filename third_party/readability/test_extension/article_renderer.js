// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  // Create a shared namespace.
  window.ReadabilityExtension = window.ReadabilityExtension || {};

  /**
   * Renders a Readability article into the provided DOM elements.
   * Assumes that dom_distiller_viewer.js has been loaded.
   * @param {object} article The article object from Readability.
   * @param {Document} doc The document to render into.
   */
  window.ReadabilityExtension.renderArticle = function(article, doc) {
    const titleHolder = doc.getElementById('title-holder');
    const contentHolder = doc.getElementById('content');

    if (article) {
      doc.title = article.title;
      titleHolder.textContent = article.title;
      contentHolder.innerHTML = article.content;

      // Set the text direction for the whole document.
      if (article.dir) {
        doc.documentElement.dir = article.dir;
      }

      // Post-process the new content using the shared helper.
      postProcessElement(contentHolder);
    } else {
      titleHolder.textContent = 'Readability could not find an article.';
      contentHolder.innerHTML = '';
    }
  };
})();

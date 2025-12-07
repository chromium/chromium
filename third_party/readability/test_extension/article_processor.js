// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  // Create a shared namespace.
  window.ReadabilityExtension = window.ReadabilityExtension || {};

  /**
   * A centralized function that invokes Readability, fetches the viewer
   * template, renders the final article HTML, and returns it as a string.
   *
   * @param {Document} doc The document to parse.
   * @return {Promise<string|null>} A promise that resolves to the fully
   *     rendered HTML string, or null if parsing fails.
   */
  window.ReadabilityExtension.processAndRenderArticle = async function(doc) {
    let article;
    try {
      // Always operate on a clone of the document to avoid side-effects.
      article = new Readability(doc.cloneNode(true)).parse();
    } catch (e) {
      console.error('Error during Readability invocation: ' + e);
      if (e.stack) {
        console.error(e.stack);
      }
      return null;
    }

    // If Readability failed to find an article, there's nothing to render.
    if (!article) {
      return null;
    }

    const viewerHtmlUrl = chrome.runtime.getURL('viewer.html');
    const commonCssUrl = chrome.runtime.getURL('distilledpage_common.css');
    const newCssUrl = chrome.runtime.getURL('distilledpage_new.css');

    // Fetch the viewer's HTML and CSS in parallel.
    const [viewerHtml, commonCss, newCss] = await Promise.all([
      fetch(viewerHtmlUrl).then(response => response.text()),
      fetch(commonCssUrl).then(response => response.text()),
      fetch(newCssUrl).then(response => response.text()),
    ]);
    const viewerCss = commonCss + '\n' + newCss;

    const parser = new DOMParser();
    const viewerDoc = parser.parseFromString(viewerHtml, 'text/html');

    // Add the viewer's CSS to the head.
    const style = viewerDoc.createElement('style');
    style.textContent = viewerCss;
    viewerDoc.head.appendChild(style);

    // Render the article into the new document.
    window.ReadabilityExtension.renderArticle(article, viewerDoc);

    // Return the fully rendered HTML.
    return viewerDoc.documentElement.outerHTML;
  }
})();

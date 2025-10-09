// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SESSION_STORAGE_KEY = 'clonedPageData';

/**
 * Renders the cloned page content into the document.
 * @param {string} html The HTML content of the page.
 * @param {object} styles The computed styles from the original page.
 */
function renderClonedPage(html, styles) {
  const parser = new DOMParser();
  const newDoc = parser.parseFromString(html, 'text/html');

  // Apply the original page's root and body styles to the new document.
  newDoc.documentElement.style.fontSize = styles.rootFontSize;
  newDoc.body.style.fontSize = styles.bodyFontSize;
  newDoc.body.style.backgroundColor = styles.bodyBackgroundColor;
  newDoc.body.style.color = styles.bodyColor;

  // Replace the current document with the cloned one.
  document.documentElement.replaceWith(newDoc.documentElement);
}

/**
 * Retrieves the cloned page data, handling both initial loads and reloads.
 * On initial load, it fetches from chrome.storage.session using the URL's
 * 'id' parameter, and then caches the data to sessionStorage. On reloads,
 * it retrieves the data directly from sessionStorage.
 * @returns {Promise<object|null>} A promise that resolves to an object
 *     containing the {html, styles} of the cloned page, or null on error.
 */
async function getClonedPageData() {
  // On refresh, the data will be in sessionStorage.
  const cachedData = sessionStorage.getItem(SESSION_STORAGE_KEY);
  if (cachedData) {
    return JSON.parse(cachedData);
  }

  // On initial load, the data is in chrome.storage.session.
  const params = new URLSearchParams(window.location.search);
  const id = params.get('id');
  if (!id) {
    document.body.textContent = 'Error: No content ID specified.';
    return null;
  }

  const result = await chrome.storage.session.get(id);
  chrome.storage.session.remove(id);

  if (!result[id]?.html) {
    document.body.textContent = 'Error: Could not find content for this page.';
    return null;
  }

  const data = result[id];

  // Store the page content in sessionStorage so it can be restored on refresh.
  sessionStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(data));

  // Clean up the URL parameter.
  history.replaceState(null, '', window.location.pathname);

  return data;
}

/******** Main Initialization ********/

(async () => {
  const data = await getClonedPageData();
  if (data) {
    renderClonedPage(data.html, data.styles);
  }
})();

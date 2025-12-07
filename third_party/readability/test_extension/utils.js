// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Consumes the 'id' from the current URL's query string. This is a
 * destructive read: it gets the ID and immediately cleans the URL to remove
 * the query parameter. This is a one-time operation for pages that receive
 * data from a background script.
 * @return {string|null} The ID found in the URL, or null if not present.
 */
function consumeIdFromUrl() {
  const params = new URLSearchParams(window.location.search);
  const id = params.get('id');
  // Clean up the URL parameter immediately.
  history.replaceState(null, '', window.location.pathname);
  return id;
}

/**
 * Retrieves a data payload from chrome.storage.session using a given ID. This
 * is a destructive read: it gets the data and immediately removes the entry
 * from storage.
 *
 * @param {string} id The ID to look up in session storage.
 * @return {Promise<object|null>} A promise that resolves to the data object
 *     stored in session storage, or null if the data cannot be found.
 */
async function consumeDataFromChromeStorage(id) {
  if (!id) {
    document.body.textContent = 'Error: No content ID specified.';
    return null;
  }

  const result = await chrome.storage.session.get(id);
  chrome.storage.session.remove(id);

  if (!result[id]) {
    document.body.textContent = 'Error: Could not find content for this page.';
    return null;
  }

  return result[id];
}

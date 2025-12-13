// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This page caches its content in sessionStorage to allow for refresh.
// To see the results of code changes, a new viewer page must be generated.

const SESSION_STORAGE_KEY = 'viewerPageData';
(async () => {
  const {installTimestamp} =
      await chrome.storage.local.get('installTimestamp');
  const cachedDataJSON = sessionStorage.getItem(SESSION_STORAGE_KEY);

  // On refresh, check if the data in sessionStorage is still valid.
  if (cachedDataJSON) {
    const cachedData = JSON.parse(cachedDataJSON);
    if (cachedData.timestamp === installTimestamp) {
      document.documentElement.innerHTML = cachedData.html;
      return;
    } else {
      // The data is stale, so clear it.
      sessionStorage.removeItem(SESSION_STORAGE_KEY);
    }
  }

  // On initial load, the data is in chrome.storage.session.
  const id = consumeIdFromUrl();
  const data = await consumeDataFromChromeStorage(id);
  if (!data?.html) {
    return;
  }

  const {html} = data;

  // Store the page content with the current install timestamp.
  sessionStorage.setItem(
      SESSION_STORAGE_KEY, JSON.stringify({html, timestamp: installTimestamp}));

  // Render the pre-rendered HTML.
  document.documentElement.innerHTML = html;
})();

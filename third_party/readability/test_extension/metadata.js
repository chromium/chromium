// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This page caches its content in sessionStorage to allow for refresh.
// To see the results of code changes, a new metadata page must be generated.

const SESSION_STORAGE_KEY = 'metadataPageData';

function renderTable(metadata) {
  const table = document.getElementById('metadata-table');
  // Clear any previous content.
  table.innerHTML = '';
  const tbody = document.createElement('tbody');

  for (const key in metadata) {
    const row = tbody.insertRow();
    const keyCell = row.insertCell();
    const valueCell = row.insertCell();

    keyCell.textContent = key;
    valueCell.textContent = metadata[key] || '(empty)';
  }

  table.appendChild(tbody);
}


(async () => {
  // On refresh, the data will be in sessionStorage.
  const cachedData = sessionStorage.getItem(SESSION_STORAGE_KEY);
  if (cachedData) {
    const metadata = JSON.parse(cachedData);
    renderTable(metadata);
    return;
  }

  // On initial load, the data is in chrome.storage.session.
  const params = new URLSearchParams(window.location.search);
  const id = params.get('id');
  if (!id) {
    document.body.textContent = 'Error: No content ID specified.';
    return;
  }

  const result = await chrome.storage.session.get(id);
  chrome.storage.session.remove(id);

  if (!result[id]) {
    document.body.textContent = 'Error: Could not find content for this page.';
    return;
  }

  const metadata = result[id];

  // Store the page content in sessionStorage so it can be restored on refresh.
  sessionStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(metadata));

  // Clean up the URL parameter.
  history.replaceState(null, '', window.location.pathname);

  renderTable(metadata);
})();

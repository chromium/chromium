// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set the installation timestamp when the extension is installed or updated.
// This is used by other pages (e.g., viewer.js) to invalidate cached
// data in `sessionStorage` and prevent stale content from being shown after
// an extension update.
chrome.runtime.onInstalled.addListener(() => {
  chrome.storage.local.set({installTimestamp: Date.now()});
});

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  // This return value indicates that the sendResponse function will be called
  // asynchronously, which keeps the message channel open.
  let willRespondAsynchronously = false;

  if (request.command === 'clone-new') {
    willRespondAsynchronously = true;
    handleClone(request.tabId, sendResponse);
  } else if (request.command === 'check-readerable') {
    willRespondAsynchronously = true;
    handleCheckReaderable(request.tabId, sendResponse);
  }

  return willRespondAsynchronously;
});

function handleClone(tabId, sendResponse) {
  chrome.scripting.executeScript(
      {target: {tabId: tabId}, files: ['extractor.js']}, (results) => {
        if (chrome.runtime.lastError || !results || !results[0]) {
          console.error(
              chrome.runtime.lastError?.message || 'Script injection failed.');
        } else {
          openClonedPageWithData(results[0].result);
        }
        // Respond to the popup to let it know it can close.
        sendResponse({});
      });
}

function handleCheckReaderable(tabId, sendResponse) {
  chrome.scripting.executeScript(
      {
        target: {tabId: tabId},
        files: ['Readability-readerable.js', 'readability_checker.js']
      },
      (results) => {
        if (chrome.runtime.lastError || !results || !results[0]) {
          console.error(
              chrome.runtime.lastError?.message || 'Script injection failed.');
          // Send a default false response on error.
          sendResponse(false);
        } else {
          // Send the boolean result back to the popup.
          sendResponse(results[0].result);
        }
      });
}

async function openClonedPageWithData(data) {
  // We can't pass the page content directly to the new tab, so we store it
  // in session storage with a temporary ID and pass that ID in the URL.
  const id = `cloned-${Date.now()}`;
  await chrome.storage.session.set({[id]: data});
  const clonedUrl = chrome.runtime.getURL(`cloned.html?id=${id}`);
  chrome.tabs.create({url: clonedUrl});
}


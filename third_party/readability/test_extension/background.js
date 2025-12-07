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

  // Commands from the popup to initiate an action.
  if (request.command === 'clone-page-new') {
    willRespondAsynchronously = true;
    handleClone(request.tabId, sendResponse);
  } else if (request.command === 'check-readerable') {
    willRespondAsynchronously = true;
    handleCheckReaderable(request.tabId, sendResponse);
  } else if (request.command === 'distill-page') {
    willRespondAsynchronously = true;
    handleDistill(request.tabId, /*inNewTab=*/ false, sendResponse);
  } else if (request.command === 'distill-page-new') {
    willRespondAsynchronously = true;
    handleDistill(request.tabId, /*inNewTab=*/ true, sendResponse);
  } else if (request.command === 'extract-metadata-new') {
    willRespondAsynchronously = true;
    handleMetadata(request.tabId, sendResponse);
  }
  // Commands from content scripts to display pre-processed data.
  else if (request.command === 'show-distilled-new') {
    displayDistilledContent(request.data, null);
  } else if (request.command === 'show-metadata-new') {
    openMetadataViewerWithData(request.data);
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

function handleDistill(tabId, inNewTab, sendResponse) {
  chrome.scripting.executeScript(
      {
        target: {tabId: tabId},
        files: [
          'Readability.js',
          'dom_distiller_viewer.js',
          'article_renderer.js',
          'article_processor.js',
          'distiller.js',
        ]
      },
      (results) => {
        if (chrome.runtime.lastError || !results || !results[0]) {
          console.error(
              chrome.runtime.lastError?.message || 'Script injection failed.');
        } else {
          const renderedHtml = results[0].result;
          if (renderedHtml) {
            const data = {html: renderedHtml};
            displayDistilledContent(data, inNewTab ? null : tabId);
          }
        }
        sendResponse({});
      });
}

function handleMetadata(tabId, sendResponse) {
  chrome.scripting.executeScript(
      {
        target: {tabId: tabId},
        files: [
          'Readability.js', 'metadata_processor.js', 'metadata_extractor.js'
        ]
      },
      (results) => {
        if (chrome.runtime.lastError || !results || !results[0]) {
          console.error(
              chrome.runtime.lastError?.message || 'Script injection failed.');
        } else {
          const metadata = results[0].result;
          if (metadata) {
            openMetadataViewerWithData(metadata);
          }
        }
        sendResponse({});
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

/**
 * Displays the given distilled content in a viewer tab.
 * @param {object} data The data to display, typically {html: '...'}.
 * @param {?number} tabId The ID of the tab to update. If null, a new tab
 *     will be created.
 */
async function displayDistilledContent(data, tabId) {
  const id = `viewer-${Date.now()}`;
  await chrome.storage.session.set({[id]: data});
  const viewerUrl = chrome.runtime.getURL(`viewer.html?id=${id}`);

  if (tabId == null) {
    chrome.tabs.create({url: viewerUrl});
  } else {
    chrome.tabs.update(tabId, {url: viewerUrl});
  }
}

async function openMetadataViewerWithData(data) {
  const id = `metadata-${Date.now()}`;
  await chrome.storage.session.set({[id]: data});
  const metadataUrl = chrome.runtime.getURL(`metadata.html?id=${id}`);
  chrome.tabs.create({url: metadataUrl});
}

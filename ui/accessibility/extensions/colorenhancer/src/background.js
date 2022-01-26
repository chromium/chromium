// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('./common.js', './storage.js');

function forEachTab(tabCallback) {
  chrome.windows.getAll({'populate': true}, windows => {
    for (const w of windows) {
      for (const tab of w.tabs) {
        if (isDisallowedUrl(tab.url)) {
          continue;
        }
        tabCallback(tab);
      }
    }
  });
}

/**
 * Adds filter script and css to all existing tabs.
 *
 * TODO(wnwen): Verify content scripts are not being injected multiple times.
 */
function injectContentScripts() {
  forEachTab(tab => chrome.scripting.executeScript({
      target: {tabId: tab.id},
      files: ['src/common.js', 'src/cvd.js'],
    }));
}

/**
 * Updates all existing tabs with config values.
 */
function updateTabs() {
  forEachTab(async function(tab) {
    const msg = {
      'delta': await getSiteDelta(siteFromUrl(tab.url)),
      'severity': await getDefaultSeverity(),
      'type': await getDefaultType(),
      'simulate': await getDefaultSimulate(),
      'enable': await getDefaultEnable()
    };
    debugPrint('updateTabs: sending ' + JSON.stringify(msg) + ' to ' +
        siteFromUrl(tab.url));
    chrome.tabs.sendMessage(tab.id, msg);
  });
}

async function onInitReceived(sender) {
  let delta;
  if (sender.tab) {
    delta = await getSiteDelta(siteFromUrl(sender.tab.url));
  } else {
    delta = await getDefaultDelta();
  }

  return {
    'delta': delta,
    'severity': await getDefaultSeverity(),
    'type': await getDefaultType(),
    'simulate': await getDefaultSimulate(),
    'enable': await getDefaultEnable()
  };
}

/**
 * Initial extension loading.
 */
(function initialize() {
  injectContentScripts();
  updateTabs();

  chrome.runtime.onMessage.addListener(
      function(message, sender, sendResponse) {
        if (message === 'init') {
          onInitReceived(sender).then(sendResponse);
          return true;  // Keep message context open for async response.
        }
      });

  //TODO(mustaq): Handle uninstall

  chrome.storage.onChanged.addListener(function() {
    updateTabs();
  });
})();

chrome.runtime.onMessage.addListener(message => {
  if (message === 'updateTabs') {
    updateTabs();
  }
});

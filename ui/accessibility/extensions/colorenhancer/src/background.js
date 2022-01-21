// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('./common.js', './storage.js');

/**
 * Adds filter script and css to all existing tabs.
 *
 * TODO(wnwen): Verify content scripts are not being injected multiple times.
 */
function injectContentScripts() {
  chrome.windows.getAll({'populate': true}, function(windows) {
    for (var i = 0; i < windows.length; i++) {
      var tabs = windows[i].tabs;
      for (var j = 0; j < tabs.length; j++) {
        var url = tabs[j].url;
        if (isDisallowedUrl(url)) {
          continue;
        }
        chrome.tabs.executeScript(
            tabs[j].id,
            {file: 'src/common.js'});
        chrome.tabs.executeScript(
            tabs[j].id,
            {file: 'src/cvd.js'});
      }
    }
  });
}

/**
 * Updates all existing tabs with config values.
 */
function updateTabs() {
  chrome.windows.getAll({'populate': true}, async function(windows) {
    for (var i = 0; i < windows.length; i++) {
      var tabs = windows[i].tabs;
      for (var j = 0; j < tabs.length; j++) {
        var url = tabs[j].url;
        if (isDisallowedUrl(url)) {
          continue;
        }
        var msg = {
          'delta': await getSiteDelta(siteFromUrl(url)),
          'severity': await getDefaultSeverity(),
          'type': await getDefaultType(),
          'simulate': await getDefaultSimulate(),
          'enable': await getDefaultEnable()
        };
        debugPrint('updateTabs: sending ' + JSON.stringify(msg) + ' to ' +
            siteFromUrl(url));
        chrome.tabs.sendMessage(tabs[j].id, msg);
      }
    }
  });
}

async function onInitReceived(sender) {
  var delta;
  if (sender.tab) {
    delta = await getSiteDelta(siteFromUrl(sender.tab.url));
  } else {
    delta = await getDefaultDelta();
  }

  var msg = {
    'delta': delta,
    'severity': await getDefaultSeverity(),
    'type': await getDefaultType(),
    'simulate': await getDefaultSimulate(),
    'enable': await getDefaultEnable()
  };
  return msg;
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

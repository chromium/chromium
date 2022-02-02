// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Storage.initialize();

function forAllTabs(tabCallback) {
  chrome.windows.getAll({'populate': true}, function(windows) {
    for (const window of windows) {
      for (const tab of window.tabs) {
        if (isDisallowedUrl(tab.url)) {
          continue;
        }
        tabCallback(tab);
      }
    }
  });
}

function injectContentScripts() {
  forAllTabs(tab => chrome.tabs.executeScript(
      tab.id,
      {file: 'highcontrast.js', allFrames: true}));
}

function updateTabs() {
  forAllTabs(tab => {
    const msg = {
      'enabled': Storage.enabled,
      'scheme': Storage.getSiteScheme(siteFromUrl(tab.url))
    };
    chrome.tabs.sendRequest(tab.id, msg);
  });
}

function toggleEnabled() {
  Storage.enabled = !Storage.enabled;
  updateTabs();
}

function toggleSite(url) {
  var site = siteFromUrl(url);
  var scheme = Storage.getSiteScheme(site);
  if (scheme > 0) {
    scheme = 0;
  } else if (Storage.scheme > 0) {
    scheme = Storage.scheme;
  } else {
    scheme = Storage.SCHEME.defaultValue;
  }
  Storage.setSiteScheme(site, scheme);
  updateTabs();
}

function init() {
  injectContentScripts();
  updateTabs();

  chrome.extension.onRequest.addListener(
      function(request, sender, sendResponse) {
        if (request['toggle_global']) {
          toggleEnabled();
        }
        if (request['toggle_site']) {
          toggleSite(sender.tab ? sender.tab.url : 'www.example.com');
        }
        if (request['init']) {
          var scheme = Storage.scheme;
          if (sender.tab) {
            scheme = Storage.getSiteScheme(siteFromUrl(sender.tab.url));
          }
          var msg = {
            'enabled': Storage.enabled,
            'scheme': scheme
          };
          sendResponse(msg);
        }
      });

  chrome.storage.onChanged.addListener(function() {
    updateTabs();
  });

  if (navigator.appVersion.indexOf('Mac') != -1) {
    chrome.browserAction.setTitle({'title': 'High Contrast (Cmd+Shift+F11)'});
  }
}

init();

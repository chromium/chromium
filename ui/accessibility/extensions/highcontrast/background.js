// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts("common.js");

function injectContentScripts() {
  chrome.windows.getAll({'populate': true}, windows => {
    for (const w of windows) {
      for (const tab of w.tabs) {
        var url = tab.url;
        if (url.indexOf('chrome') == 0 || url.indexOf('about') == 0) {
          continue;
        }
        chrome.scripting.executeScript({
            target: { tabId: tab.id, allFrames: true },
            files: [ 'common.js', 'highcontrast.js' ],
            injectImmediately: true,
        });
      }
    }
  });
}

function toggleEnabled() {
  setEnabled(!getEnabled());
}

function toggleSite(url) {
  var site = siteFromUrl(url);
  var scheme = getSiteScheme(site);
  if (scheme > 0) {
    scheme = 0;
  } else if (getDefaultScheme() > 0) {
    scheme = getDefaultScheme();
  } else {
    scheme = DEFAULT_SCHEME;
  }
  setSiteScheme(site, scheme);
}

async function init() {
  injectContentScripts();

  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message['toggle_global']) {
      toggleEnabled();
    }
    if (message['toggle_site']) {
      toggleSite(sender.tab ? sender.tab.url : 'www.example.com');
    }
  });

  if (navigator.appVersion.indexOf('Mac') != -1) {
    chrome.action.setTitle({'title': 'High Contrast (Cmd+Shift+F11)'});
  }
}

self.addEventListener('install', init);

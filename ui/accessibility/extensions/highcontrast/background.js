// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Storage.initialize();

function injectContentScripts() {
  chrome.windows.getAll({'populate': true}, function(windows) {
    for (var i = 0; i < windows.length; i++) {
      var tabs = windows[i].tabs;
      for (var j = 0; j < tabs.length; j++) {
        var url = tabs[j].url;
        if (url.startsWith('chrome') || url.startsWith('about'))
          continue;

        chrome.tabs.executeScript(
            tabs[j].id,
            {file: 'highcontrast.js', allFrames: true});
      }
    }
  });
}

function updateTabs() {
  var msg = {
    'enabled': Storage.enabled
  };
  chrome.windows.getAll({'populate': true}, function(windows) {
    for (var i = 0; i < windows.length; i++) {
      var tabs = windows[i].tabs;
      for (var j = 0; j < tabs.length; j++) {
        var url = tabs[j].url;
        if (isDisallowedUrl(url)) {
          continue;
        }
        var msg = {
          'enabled': Storage.enabled,
          'scheme': Storage.getSiteScheme(siteFromUrl(url))
        };
        chrome.tabs.sendRequest(tabs[j].id, msg);
      }
    }
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

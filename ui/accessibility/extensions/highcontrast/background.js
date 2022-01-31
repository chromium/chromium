// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    'enabled': getEnabled()
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
          'enabled': getEnabled(),
          'scheme': getSiteScheme(siteFromUrl(url))
        };
        chrome.tabs.sendRequest(tabs[j].id, msg);
      }
    }
  });
}

function toggleEnabled() {
  setEnabled(!getEnabled());
  updateTabs();
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
          var scheme = getDefaultScheme();
          if (sender.tab) {
            scheme = getSiteScheme(siteFromUrl(sender.tab.url));
          }
          var msg = {
            'enabled': getEnabled(),
            'scheme': scheme
          };
          sendResponse(msg);
        }
      });

  document.addEventListener('storage', function(evt) {
    updateTabs();
  }, false);

  if (navigator.appVersion.indexOf('Mac') != -1) {
    chrome.browserAction.setTitle({'title': 'High Contrast (Cmd+Shift+F11)'});
  }
}

init();

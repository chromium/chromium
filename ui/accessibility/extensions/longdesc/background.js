/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

var ariaDescribedAt = '';
var longDesc = '';

 /**
 * This is called when the extension is first loaded, so that it can be
 * immediately used in all already-open tabs. It's not needed for any
 * new tabs that open after that, the content script will be automatically
 * injected into any new tab.
 */
chrome.windows.getAll({'populate': true}, function(windows) {
  for (var i = 0; i < windows.length; i++) {
    var tabs = windows[i].tabs;
    for (var j = 0; j < tabs.length; j++) {
      try {
        chrome.tabs.insertCSS(
          tabs[j].id,
          {file: 'border.css'},
          function(result) {
            chrome.runtime.lastError;
          });
      } catch (x) {
      }
      try {
        chrome.tabs.executeScript(
          tabs[j].id,
          {file: 'lastRightClick.js'},
          function(result) {
            chrome.runtime.lastError;
          });
      } catch (x) {
      }
    }
  }
});

/**
 * Add context menu item when the extension is installed.
 */
chrome.contextMenus.onClicked.addListener(contextMenuClicked);
chrome.contextMenus.create({
    "title": chrome.i18n.getMessage('longdesc_context_menu_item'),
    "contexts": ["all"],
    "id": "moreInfo",
    "enabled": false
  }, onMenuReady);

/**
 * Add listener for messages from content script.
 * Enable/disable the context menu item.
 */
chrome.runtime.onMessage.addListener(
  function (request, sender, sendResponse) {
    if (request.enabled) {
      ariaDescribedAt = request.ariaDescribedAt;
      longDesc = request.longDesc;
    }
    if (globalThis.menuReady) {
      chrome.contextMenus.update('moreInfo', {
        "enabled": request.enabled
      });
    }
  });

function onMenuReady() {
  globalThis.menuReady = true;
}

/**
 * Event handler for when a context menu item is clicked.
 * aria-describedat is given a higher priority.
 * No need to strip the URL of leading/trailing white space
 * because Chrome takes care of this.
 *
 * @param info
 * @param tab
 */
function contextMenuClicked(info, tab) {
  if (ariaDescribedAt !== '') {
    chrome.tabs.create({url: ariaDescribedAt});
  } else if (longDesc !== '') {
    chrome.tabs.create({url: longDesc});
  }
}

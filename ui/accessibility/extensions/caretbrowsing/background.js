// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script that runs on the background page.
 */

importScripts('storage.js');
Storage.initialize();

CONTENT_SCRIPTS = [
  'accessibility_utils.js', 'node_util.js', 'selection_util.js',
  'traverse_util.js', 'storage.js', 'caret_browsing.js'
];

/**
 * The class handling the Caret Browsing background page, which keeps
 * track of the current state, handles the browser action button, and
 * initializes the content script in all running tabs when the extension
 * is first loaded.
 * @constructor
 */
const CaretBkgnd = function() {};

/**
 * Change the browser action icon and tooltip based on the enabled state.
 */
CaretBkgnd.setIcon = function() {
  chrome.action.setIcon(
      {'path': Storage.enabled ?
               '../caret_19_on.png' :
               '../caret_19.png'});
  chrome.action.setTitle(
      {'title': Storage.enabled ?
                'Turn Off Caret Browsing (F7)' :
                'Turn On Caret Browsing (F7)' });
};

/**
 * This is called when the extension is first loaded, so that it can be
 * immediately used in all already-open tabs. It's not needed for any
 * new tabs that open after that, the content script will be automatically
 * injected into any new tab.
 */
CaretBkgnd.injectContentScripts = function() {
  chrome.windows.getAll({'populate': true}, function(windows) {
    for (const w of windows) {
      for (const tab of w.tabs) {
        chrome.scripting.executeScript(
            {
              target: {tabId: tab.id, allFrames: true},
              files: CONTENT_SCRIPTS,
            },
            function(result) {
              // Ignore.
              chrome.runtime.lastError;
            });
      }
    }
  });
};

/**
 * Toggle caret browsing on or off, and update the browser action icon and
 * all open tabs.
 */
CaretBkgnd.toggle = function() {
  Storage.enabled = !Storage.enabled;
  CaretBkgnd.setIcon();
};

/**
 * Initialize the background script. Set the initial value of the flag
 * based on the saved preference in localStorage, update the browser action,
 * inject into running tabs, and then set up communication with content
 * scripts in tabs. Also check for prefs updates (from the options page)
 * and send them to content scripts.
 */
CaretBkgnd.init = function() {
  CaretBkgnd.setIcon();
  chrome.action.onClicked.addListener(CaretBkgnd.toggle);
  Storage.ENABLED.listeners.push(CaretBkgnd.setIcon);
};

CaretBkgnd.init();
self.addEventListener('install', CaretBkgnd.injectContentScripts);

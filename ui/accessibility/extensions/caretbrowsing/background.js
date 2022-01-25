// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script that runs on the background page.
 */

CONTENT_SCRIPTS = [
  'accessibility_utils.js',
  'traverse_util.js',
  'caret_browsing.js'
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
 * Flag indicating whether caret browsing is enabled. Global, applies to
 * all tabs simultaneously.
 * @type {boolean}
 */
CaretBkgnd.isEnabled;

/**
 * Change the browser action icon and tooltip based on the enabled state.
 */
CaretBkgnd.setIcon = function() {
  chrome.browserAction.setIcon(
      {'path': CaretBkgnd.isEnabled ?
               '../caret_19_on.png' :
               '../caret_19.png'});
  chrome.browserAction.setTitle(
      {'title': CaretBkgnd.isEnabled ?
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
    for (let i = 0; i < windows.length; i++) {
      const tabs = windows[i].tabs;
      for (let j = 0; j < tabs.length; j++) {
        for (let k = 0; k < CONTENT_SCRIPTS.length; k++) {
          chrome.tabs.executeScript(
              tabs[j].id,
              {file: CONTENT_SCRIPTS[k], allFrames: true},
              function(result) {
                // Ignore.
                chrome.runtime.lastError;
              });
        }
      }
    }
  });
};

/**
 * Toggle caret browsing on or off, and update the browser action icon and
 * all open tabs.
 */
CaretBkgnd.toggle = function() {
  CaretBkgnd.isEnabled = !CaretBkgnd.isEnabled;
  var obj = {};
  obj['enabled'] = CaretBkgnd.isEnabled;
  chrome.storage.sync.set(obj);
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
  chrome.storage.sync.get('enabled', function(result) {
    CaretBkgnd.isEnabled = result['enabled'];
    CaretBkgnd.setIcon();
    CaretBkgnd.injectContentScripts();

    chrome.browserAction.onClicked.addListener(function(tab) {
      CaretBkgnd.toggle();
    });
  });

  chrome.storage.onChanged.addListener(function() {
    chrome.storage.sync.get('enabled', function(result) {
      CaretBkgnd.isEnabled = result['enabled'];
      CaretBkgnd.setIcon();
    });
  });
};

CaretBkgnd.init();

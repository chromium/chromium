// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('./common.js', './storage.js');

class Background {
  constructor() {
    /** @private {Storage} */
    this.storage_ = new Storage();

    this.init_();
  }

  /**
   * @param {function(chrome.tabs.Tab)} tabCallback A function that performs
   *     an action on each tab
   * @private
   */
  forEachTab_(tabCallback) {
    chrome.windows.getAll({'populate': true}, windows => {
      for (const w of windows) {
        for (const tab of w.tabs) {
          if (Common.isDisallowedUrl(tab.url)) {
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
  injectContentScripts() {
    this.forEachTab_(tab => chrome.scripting.executeScript({
      target: {tabId: tab.id},
      files: ['src/common.js', 'src/matrix.js', 'src/cvd.js'],
    }));
  }

  /**
   * Updates all existing tabs with config values.
   * @private
   */
  updateTabs_() {
    this.forEachTab_((tab) => {
      const msg = {
        'delta': this.storage_.getSiteDelta(Common.siteFromUrl(tab.url)),
        'severity': this.storage_.getDefaultSeverity(),
        'type': this.storage_.getDefaultType(),
        'simulate': this.storage_.getDefaultSimulate(),
        'enable': this.storage_.getDefaultEnable()
      };
      Common.debugPrint(
          'updateTabs: sending ' + JSON.stringify(msg) + ' to ' +
          Common.siteFromUrl(tab.url));
      chrome.tabs.sendMessage(tab.id, msg);
    });
  }

  /** @private */
  onInitReceived_(sender) {
    let delta;
    if (sender.tab) {
      delta = this.storage_.getSiteDelta(Common.siteFromUrl(sender.tab.url));
    } else {
      delta = this.storage_.getDefaultDelta();
    }

    return {
      'delta': delta,
      'severity': this.storage_.getDefaultSeverity(),
      'type': this.storage_.getDefaultType(),
      'simulate': this.storage_.getDefaultSimulate(),
      'enable': this.storage_.getDefaultEnable()
    };
  }

  /**
   * Initial extension loading.
   * @private
   */
  init_() {
    this.updateTabs_();

    chrome.runtime.onMessage.addListener(
        (message, sender, sendResponse) => {
          if (message === 'init') {
            this.onInitReceived_(sender);
            sendResponse();
            return true;  // Keep message context open for async response.
          } else if (message === 'updateTabs') {
            this.updateTabs_();
          }
        });
    chrome.storage.onChanged.addListener(this.updateTabs_.bind(this));
    //TODO(mustaq): Handle uninstall
  }
}

const background = new Background();
self.addEventListener(
    'install', background.injectContentScripts.bind(background));

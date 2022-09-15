// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('./common.js', './cvd_type.js', './storage.js');
Storage.initialize();

class Background {
  constructor() {
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
      files: [
          'src/common.js', 'src/matrix.js', 'src/cvd_type.js', 'src/cvd.js'],
    }));
  }

  /**
   * Updates all existing tabs with config values.
   * @private
   */
  updateTabs_() {
    this.forEachTab_((tab) => {
      const msg = {
        'delta': Storage.getSiteDelta(Common.siteFromUrl(tab.url)),
        'severity': Storage.severity,
        'type': Storage.type,
        'simulate': Storage.simulate,
        'enable': Storage.enable,
        'axis': Storage.axis
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
      delta = Storage.getSiteDelta(Common.siteFromUrl(sender.tab.url));
    } else {
      delta = Storage.baseDelta;
    }

    return {
      'delta': delta,
      'severity': Storage.severity,
      'type': Storage.type,
      'simulate': Storage.simulate,
      'enable': Storage.enable,
      'axis': Storage.axis
    };
  }

  /**
   * Initial extension loading.
   * @private
   */
  init_() {
    Storage.DELTA.listeners.push(this.updateTabs_.bind(this));
    Storage.SITE_DELTAS.listeners.push(this.updateTabs_.bind(this));
    Storage.SEVERITY.listeners.push(this.updateTabs_.bind(this));
    Storage.TYPE.listeners.push(this.updateTabs_.bind(this));
    Storage.SIMULATE.listeners.push(this.updateTabs_.bind(this));
    Storage.ENABLE.listeners.push(this.updateTabs_.bind(this));
    Storage.AXIS.listeners.push(this.updateTabs_.bind(this));

    this.updateTabs_();

    chrome.runtime.onMessage.addListener(
        (message, sender, sendResponse) => {
          if (message === 'init') {
            this.onInitReceived_(sender);
            sendResponse();
          }
        });
    //TODO(mustaq): Handle uninstall
  }
}

const background = new Background();
self.addEventListener(
    'install', background.injectContentScripts.bind(background));

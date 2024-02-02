// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Background {
  constructor() {
    this.init_();
  }

  /**
   * @param {function(chrome.tabs.Tab)} tabCallback A function that performs
   *     some action on each tab.
   * @private
   */
  forAllTabs_(tabCallback) {
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

  /** @public */
  injectContentScripts() {
    this.forAllTabs_(tab => {
      try {
        chrome.scripting.executeScript({
          target: {tabId: tab.id, allFrames: true},
          files: ['highcontrast.js'],
        });
      } catch (err) {
        // Convert the error to a warning to prevent service worker crash.
        console.warn(err);
      }
    });
  }

  /** @private */
  updateTabs_() {
    this.forAllTabs_(tab => {
      const msg = {
        'enabled': Storage.enabled,
        'scheme': Storage.getSiteScheme(siteFromUrl(tab.url)),
      };
      chrome.tabs.sendMessage(tab.id, msg);
    });
  }

  /** @private */
  toggleEnabled_() {
    Storage.enabled = !Storage.enabled;
    this.updateTabs_();
  }

  /**
   * @param {string} url
   * @private
   */
  toggleSite_(url) {
    const site = siteFromUrl(url);
    let scheme = Storage.getSiteScheme(site);
    if (scheme > 0) {
      scheme = 0;
    } else if (Storage.scheme > 0) {
      scheme = Storage.scheme;
    } else {
      scheme = Storage.SCHEME.defaultValue;
    }
    Storage.setSiteScheme(site, scheme);
    this.updateTabs_();
  }

  /**
   * @param {*} message
   * @param {chrome.runtime.MessageSender} sender
   * @param {function} sendResponse
   * @private
   */
  handleRequest_(message, sender, sendResponse) {
    if (message['updateTabs']) {
      this.updateTabs_();
    }
    if (message['toggle_global']) {
      this.toggleEnabled_();
    }
    if (message['toggle_site']) {
      this.toggleSite_(sender.tab ? sender.tab.url : 'www.example.com');
    }
    if (message['init']) {
      let scheme = Storage.scheme;
      if (sender.tab) {
        scheme = Storage.getSiteScheme(siteFromUrl(sender.tab.url));
      }
      const msg = {
        'enabled': Storage.enabled,
        'scheme': scheme,
      };
      sendResponse(msg);
    }
  }

  /** @private */
  init_() {
    this.injectContentScripts();
    this.updateTabs_();

    chrome.runtime.onMessage.addListener(this.handleRequest_.bind(this));

    chrome.storage.onChanged.addListener(this.updateTabs_.bind(this));

    if (navigator.appVersion.indexOf('Mac') != -1) {
      chrome.browserAction.setTitle({'title': 'High Contrast (Cmd+Shift+F11)'});
    }
  }
}

self.addEventListener('install', () => {
  importScripts('common.js');
  importScripts('storage.js');
  Storage.initialize();
  const background = new Background();
});

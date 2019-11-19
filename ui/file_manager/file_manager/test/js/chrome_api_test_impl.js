// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test implementation of chrome.* apis.
 *
 * These APIs are provided natively to a chrome app, but since we are
 * running as a regular web page, we must provide test implementations.
 */

// All testing functions in namespace 'test'.
var test = test || {};

test.Event = class {
  constructor() {
    this.listeners_ = [];
  }

  /** @param {function()} callback */
  addListener(callback) {
    this.listeners_.push(callback);
  }

  /** @param {function()} callback */
  removeListener(callback) {
    this.listeners_ = this.listeners_.filter(l => l !== callback);
  }

  /** @param {...*} args */
  dispatchEvent(...args) {
    setTimeout(() => {
      for (let listener of this.listeners_) {
        listener(...args);
      }
    }, 0);
  }
};

/**
 * Suppress compiler warning for overwriting default chrome object.
 * @suppress {checkTypes|const}
 */
chrome = {
  app: {
    runtime: {
      onLaunched: new test.Event(),
      onRestarted: new test.Event(),
    },
    window: {
      current: () => {
        return window;
      },
    },
  },

  commandLinePrivate: {
    switches_: {},
    hasSwitch: (name, callback) => {
      setTimeout(callback, 0, chrome.commandLinePrivate.switches_[name]);
    },
  },

  contextMenus: {
    create: () => {},
    onClicked: new test.Event(),
  },

  echoPrivate: {
    getOfferInfo: (id, callback) => {
      setTimeout(() => {
        // checkSpaceAndMaybeShowWelcomeBanner_ relies on lastError being set.
        chrome.runtime.lastError = {message: 'Not found'};
        callback(undefined);
        delete chrome.runtime.lastError;
      }, 0);
    },
  },

  extension: {
    getViews: (fetchProperties) => {
      // Returns Window[].
      return [window];
    },
    inIncognitoContext: false,
  },

  fileBrowserHandler: {
    onExecute: new test.Event(),
  },

  i18n: {
    getMessage: (messageName, opt_substitutions) => {
      return messageName;
    },
  },

  metricsPrivate: {
    userActions_: [],
    smallCounts_: [],
    times_: [],
    values_: [],
    MetricTypeType: {
      HISTOGRAM_LINEAR: 'histogram-linear',
    },
    recordMediumCount: () => {},
    recordPercentage: () => {},
    recordSmallCount: (metricName, value) => {
      chrome.metricsPrivate.smallCounts_.push([metricName, value]);
    },
    recordTime: (metricName, value) => {
      chrome.metricsPrivate.times_.push([metricName, value]);
    },
    recordUserAction: (action) => {
      chrome.metricsPrivate.userActions_.push(action);
    },
    recordValue: (metricName, value) => {
      chrome.metricsPrivate.values_.push([metricName, value]);
    },
  },

  notifications: {
    onButtonClicked: new test.Event(),
    onClicked: new test.Event(),
    onClosed: new test.Event(),
  },

  power: {
    requestKeepAwake: (level) => {},
    releaseKeepAwake: () => {},
  },

  runtime: {
    getBackgroundPage: (callback) => {
      setTimeout(callback, 0, window);
    },
    // FileManager extension ID.
    id: 'hhaomjibdihmijegdhdafkllkbggdgoj',
    onMessageExternal: new test.Event(),
    sendMessage: (extensionId, message, options, opt_callback) => {
      // Returns JSON.
      if (opt_callback) {
        setTimeout(opt_callback(''), 0);
      }
    },
  },

  storage: {
    state_: {},
    local: {
      get: (keys, callback) => {
        var keys = keys instanceof Array ? keys : [keys];
        var result = {};
        keys.forEach(key => {
          if (key in chrome.storage.state_) {
            result[key] = chrome.storage.state_[key];
          }
        });
        setTimeout(callback, 0, result);
      },
      set: (items, opt_callback) => {
        for (var key in items) {
          chrome.storage.state_[key] = items[key];
        }
        if (opt_callback) {
          setTimeout(opt_callback, 0);
        }
      },
    },
    onChanged: new test.Event(),
    sync: {
      get: (keys, callback) => {
        setTimeout(callback, 0, {});
      }
    },
  },
};

// cws_widget_container.js loads the chrome web store widget as
// a WebView.  It calls WebView.request.onBeforeSendHeaders.
HTMLElement.prototype.request = {
  onBeforeSendHeaders: new test.Event(),
};

// cws_widget_container.js also calls WebView.stop.
HTMLElement.prototype.stop = () => {};

/**
 * domAutomationController is provided in tests, but is useful for debugging
 * tests in a browser.  We use jsdoc extends rather than 'class ... extends ...'
 * since DomAutomationController does not exist at runtime in a browser.
 * @extends {DomAutomationController}
 */
class ConsoleDomAutomationController {
  send(json) {
    console.debug('domAutomationController.send', json);
  }
}

window.domAutomationController =
    window.domAutomationController || new ConsoleDomAutomationController();

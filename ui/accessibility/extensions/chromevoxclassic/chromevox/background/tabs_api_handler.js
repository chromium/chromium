// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accesses Chrome's tabs extension API and gives
 * feedback for events that happen in the "Chrome of Chrome".
 */

goog.provide('cvox.TabsApiHandler');

goog.require('TabsAutomationHandler');
goog.require('cvox.AbstractEarcons');
goog.require('cvox.AbstractTts');
goog.require('cvox.BrailleInterface');
goog.require('cvox.ChromeVox');
goog.require('cvox.NavBraille');


/**
 * Class that adds listeners and handles events from the tabs API.
 * @constructor
 */
cvox.TabsApiHandler = function() {
  /** @type {function(string, Array<string>=)} @private */
  this.msg_ = Msgs.getMsg.bind(Msgs);
  /**
   * Tracks whether the active tab has finished loading.
   * @type {boolean}
   * @private
   */
  this.lastActiveTabLoaded_ = false;

  chrome.tabs.onCreated.addListener(this.onCreated.bind(this));
  chrome.tabs.onRemoved.addListener(this.onRemoved.bind(this));
  chrome.tabs.onActivated.addListener(this.onActivated.bind(this));
  chrome.tabs.onUpdated.addListener(this.onUpdated.bind(this));
  chrome.windows.onFocusChanged.addListener(this.onFocusChanged.bind(this));

  /**
   * @type {?number} The window.setInterval ID for checking the loading
   *     status of the current tab.
   * @private
   */
  this.pageLoadIntervalID_ = null;

  /**
   * @type {?number} The tab ID of the tab being polled because it's loading.
   * @private
   */
  this.pageLoadTabID_ = null;
};

/**
 * @type {boolean}
 */
cvox.TabsApiHandler.shouldOutputSpeechAndBraille = true;

cvox.TabsApiHandler.prototype = {
  /**
   * Handles chrome.tabs.onCreated.
   * @param {Object} tab
   */
  onCreated: function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    if (cvox.TabsApiHandler.shouldOutputSpeechAndBraille) {
      cvox.ChromeVox.tts.speak(this.msg_('chrome_tab_created'),
                                cvox.QueueMode.FLUSH,
                                cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
      cvox.ChromeVox.braille.write(
          cvox.NavBraille.fromText(this.msg_('chrome_tab_created')));
    }
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_OPEN);
    if (tab) {
      this.refreshAutomationHandler_(tab.id);
    }
  },

  /**
   * Handles chrome.tabs.onRemoved.
   * @param {Object} tab
   */
  onRemoved: function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_CLOSE);

    chrome.tabs.query({active: true}, function(tabs) {
      if (tabs.length == 0 && this.isPlayingPageLoadingSound_()) {
        cvox.ChromeVox.earcons.cancelEarcon(cvox.Earcon.PAGE_START_LOADING);
        this.cancelPageLoadTimer_();
      }
    }.bind(this));
  },

  /**
   * Handles chrome.tabs.onActivated.
   * @param {Object} activeInfo
   */
  onActivated: function(activeInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.updateLoadingSoundsWhenTabFocusChanges_(activeInfo.tabId);
    chrome.tabs.get(activeInfo.tabId, function(tab) {
      if (cvox.TabsApiHandler.shouldOutputSpeechAndBraille) {
        var title = tab.title ? tab.title : tab.url;
        cvox.ChromeVox.tts.speak(this.msg_('chrome_tab_selected',
                                           [title]),
                                 cvox.QueueMode.FLUSH,
                                 cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
        cvox.ChromeVox.braille.write(
            cvox.NavBraille.fromText(
                this.msg_('chrome_tab_selected', [title])));
      }
      cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_SELECT);
      this.refreshAutomationHandler_(tab.id);
      this.focusTab_(tab.id);
    }.bind(this));
  },

  /**
   * Called when a tab becomes active or focused.
   * @param {number} tabId the id of the tab that's now focused and active.
   * @private
   */
  updateLoadingSoundsWhenTabFocusChanges_: function(tabId) {
    chrome.tabs.get(tabId, function(tab) {
      this.lastActiveTabLoaded_ = tab.status == 'complete';
      if (tab.status == 'loading' && !this.isPlayingPageLoadingSound_()) {
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.PAGE_START_LOADING);
        this.startPageLoadTimer_(tabId);
      } else {
        cvox.ChromeVox.earcons.cancelEarcon(cvox.Earcon.PAGE_START_LOADING);
        this.cancelPageLoadTimer_();
      }
    }.bind(this));
  },

  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} selectInfo
   */
  onUpdated: function(tabId, selectInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.active) {
        return;
      }
      if (tab.status == 'loading') {
        this.lastActiveTabLoaded_ = false;
        if (!this.isPlayingPageLoadingSound_()) {
          cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.PAGE_START_LOADING);
          this.startPageLoadTimer_(tabId);
        }
      } else if (!this.lastActiveTabLoaded_) {
        this.lastActiveTabLoaded_ = true;
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.PAGE_FINISH_LOADING);
        this.cancelPageLoadTimer_();
      }
      this.refreshAutomationHandler_(tabId);
    }.bind(this));
  },

  /**
   * Handles chrome.windows.onFocusChanged.
   * @param {number} windowId
   */
  onFocusChanged: function(windowId) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    if (windowId == chrome.windows.WINDOW_ID_NONE) {
      return;
    }
    chrome.windows.get(windowId, function(window) {
      chrome.tabs.query({active: true, windowId: windowId}, function(tabs) {
        if (tabs[0])
          this.updateLoadingSoundsWhenTabFocusChanges_(tabs[0].id);

        if (cvox.TabsApiHandler.shouldOutputSpeechAndBraille) {
          var msgId = window.incognito ? 'chrome_incognito_window_selected' :
              'chrome_normal_window_selected';
          var tab = tabs[0] || {};
          var title = tab.title ? tab.title : tab.url;
          cvox.ChromeVox.tts.speak(this.msg_(msgId, [title]),
                                   cvox.QueueMode.FLUSH,
                                   cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
          cvox.ChromeVox.braille.write(
              cvox.NavBraille.fromText(this.msg_(msgId, [title])));
        }
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_SELECT);
        this.refreshAutomationHandler_(tab.id);
        this.focusTab_(tab.id);
      }.bind(this));
    }.bind(this));
  },

  /**
   * Installs a new automation handler for the given tab.
   * @param {number} tabId
   * @private
   */
  refreshAutomationHandler_: function(tabId) {
    if (!cvox.ChromeVox.isMac)
      return;

    chrome.automation.getTree(tabId, function(node) {
      if (this.handler_)
        this.handler_.removeAllListeners();

      this.handler_ = new TabsAutomationHandler(node);
    }.bind(this));
  },

  /**
   * @param {number} id Tab id to focus.
   * @private
   */
  focusTab_: function(id) {
    chrome.automation.getTree(id, function(tab) {
      if (!tab)
        return;

      ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(tab));
    });
  },

  /**
   * The chrome.tabs API doesn't always fire an onUpdated event when a
   * page finishes loading, so we poll it.
   * @param {number} tabId The id of the tab to monitor.
   * @private
   */
  startPageLoadTimer_: function(tabId) {
    if (this.pageLoadIntervalID_) {
      if (tabId == this.pageLoadTabID_)
        return;
      this.cancelPageLoadTimer_();
    }

    this.pageLoadTabID_ = tabId;
    this.pageLoadIntervalID_ = window.setInterval(function() {
      if (this.pageLoadTabID_)
        this.onUpdated(this.pageLoadTabID_, {});
    }.bind(this), 1000);
  },

  /**
   * Cancel the page loading timer because the active tab is loaded.
   * @private
   */
  cancelPageLoadTimer_: function() {
    if (this.pageLoadIntervalID_) {
      window.clearInterval(this.pageLoadIntervalID_);
      this.pageLoadIntervalID_ = null;
      this.pageLoadTabID_ = null;
    }
  },

  /**
   * @return {boolean} True if the page loading sound is playing and our
   * page loading timer is active.
   */
  isPlayingPageLoadingSound_: function() {
    return this.pageLoadIntervalID_ != null;
  }
};

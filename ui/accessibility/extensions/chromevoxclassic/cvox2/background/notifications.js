// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides notification support for ChromeVox.
 */

goog.provide('Notifications');

/**
 * ChromeVox update notification.
 * @constructor
 */
function UpdateNotification() {
  this.data = {};
  this.data.type = 'basic';
  this.data.iconUrl = '/images/chromevox-16.png';
  this.data.title = Msgs.getMsg('update_title');
  this.data.message = Msgs.getMsg('update_message_next');
}

UpdateNotification.prototype = {
  /** @return {boolean} */
  shouldShow: function() {
    return !localStorage['notifications_update_notification_shown'] &&
        chrome.runtime.getManifest().version >= '53';
  },

  /** Shows the notification. */
  show: function() {
    if (!this.shouldShow())
      return;
    chrome.notifications.create('update', this.data);
    chrome.notifications.onClicked.addListener(this.onClicked.bind(this));
    chrome.notifications.onClosed.addListener(this.onClosed.bind(this));
  },

  /**
   * Handles the chrome.notifications event.
   * @param {string} notificationId
   */
  onClicked: function(notificationId) {
    var nextUpdatePage = {url: 'cvox2/background/next_update.html'};
    chrome.tabs.create(nextUpdatePage);
  },

  /**
   * Handles the chrome.notifications event.
   * @param {string} id
   */
  onClosed: function(id) {
    localStorage['notifications_update_notification_shown'] = true;
  }
};

/**
 * Runs notifications that should be shown for startup.
 */
Notifications.onStartup = function() {
  // Only run on background page.
  if (document.location.href.indexOf('background.html') == -1)
    return;

  new UpdateNotification().show();
};

/**
 * Runs notifications that should be shown for mode changes.
 */
Notifications.onModeChange = function() {
  // Only run on background page.
  if (document.location.href.indexOf('background.html') == -1)
    return;

  if (ChromeVoxState.instance.mode !== ChromeVoxMode.FORCE_NEXT)
    return;

  new UpdateNotification().show();
};

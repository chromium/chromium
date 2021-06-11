// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper class used as a browser proxy for the
 * chrome.notifications browser API.
 */

/** @interface */
export class NotificationsBrowserProxy {
  /**
   * @param {string} notificationId
   * @param {function(boolean)} callback
   */
  clear(notificationId, callback) {}

  /**
   * @param {string} notificationId
   * @param {NotificationOptions} options
   * @param {function(string)} callback
   */
  create(notificationId, options, callback) {}

  /**
   * @param {function(Object)} callback
   */
  getAll(callback) {}

  /**
   * @param {function(string)} callback
   */
  getPermissionLevel(callback) {}

  /**
   * @param {string} notificationId
   * @param {NotificationOptions} options
   * @param {function(boolean)} callback
   */
  update(notificationId, options, callback) {}
}

/**
 * The events that we need to attach listeners to.
 * @readonly
 * @enum {string}
 */
const NotificationEventTypes = {
  CLICKED: 'onClicked',
  BUTTON_CLICKED: 'onButtonClicked',
  CLOSED: 'onClosed'
};

Object.freeze(NotificationEventTypes);

class SystemNotificationEvent {
  constructor(eventType, generateSystemNotifications) {
    this.eventType = eventType;
    this.generateSystemNotifications = generateSystemNotifications;
  }

  addListener(callback) {
    if (this.generateSystemNotifications) {
      switch (this.eventType) {
        case NotificationEventTypes.CLICKED:
          chrome.notifications.onClicked.addListener(callback);
          break;
        case NotificationEventTypes.BUTTON_CLICKED:
          chrome.notifications.onButtonClicked.addListener(callback);
          break;
        case NotificationEventTypes.CLOSED:
          chrome.notifications.onClosed.addListener(callback);
          break;
      }
    }
  }
}

/**
 * @implements {NotificationsBrowserProxy}
 */
export class NotificationsBrowserProxyImpl {
  constructor(generateSystemNotifications) {
    this.generateSystemNotifications = generateSystemNotifications;
    this.onClicked =
        new SystemNotificationEvent('onClicked', generateSystemNotifications);
    this.onButtonClicked = new SystemNotificationEvent(
        'onButtonClicked', generateSystemNotifications);
    this.onClosed =
        new SystemNotificationEvent('onClosed', generateSystemNotifications);
  }

  /** @override */
  clear(notificationId, callback) {
    if (this.generateSystemNotifications) {
      chrome.notifications.clear(notificationId, callback);
    }
  }

  /** @override */
  create(notificationId, options, callback) {
    if (this.generateSystemNotifications) {
      chrome.notifications.create(notificationId, options, callback);
    }
  }

  /** @override */
  getAll(callback) {
    if (this.generateSystemNotifications) {
      chrome.notifications.getAll(callback);
    }
  }

  /** @override */
  getPermissionLevel(callback) {
    if (this.generateSystemNotifications) {
      chrome.notifications.getPermissionLevel(callback);
    }
  }

  /** @override */
  update(notificationId, options, callback) {
    if (this.generateSystemNotifications) {
      chrome.notifications.update(notificationId, options, callback);
    }
  }
}

export const notifications = new NotificationsBrowserProxyImpl(!window.isSWA);

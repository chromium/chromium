// Copyright 2021 The Chromium Authors
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

  /**
   * @param {boolean} enabled
   */
  setSystemNotificationEnabled(enabled) {}
}

/**
 * The events that we need to attach listeners to.
 * @readonly
 * @enum {string}
 */
const NotificationEventTypes = {
  CLICKED: 'onClicked',
  BUTTON_CLICKED: 'onButtonClicked',
  CLOSED: 'onClosed',
};

Object.freeze(NotificationEventTypes);

class SystemNotificationEvent {
  constructor(eventType, systemNotificationEnabled) {
    this.eventType = eventType;
    this.systemNotificationEnabled = systemNotificationEnabled;
  }

  addListener(callback) {
    if (this.systemNotificationEnabled) {
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
  constructor(systemNotificationEnabled) {
    this.systemNotificationEnabled = systemNotificationEnabled;
    this.onClicked =
        new SystemNotificationEvent('onClicked', systemNotificationEnabled);
    this.onButtonClicked = new SystemNotificationEvent(
        'onButtonClicked', systemNotificationEnabled);
    this.onClosed =
        new SystemNotificationEvent('onClosed', systemNotificationEnabled);
  }

  /** @override */
  clear(notificationId, callback) {
    if (this.systemNotificationEnabled) {
      chrome.notifications.clear(notificationId, callback);
    }
  }

  /** @override */
  create(notificationId, options, callback) {
    if (this.systemNotificationEnabled) {
      chrome.notifications.create(notificationId, options, callback);
    }
  }

  /** @override */
  getAll(callback) {
    if (this.systemNotificationEnabled) {
      chrome.notifications.getAll(callback);
    }
  }

  /** @override */
  getPermissionLevel(callback) {
    if (this.systemNotificationEnabled) {
      chrome.notifications.getPermissionLevel(callback);
    }
  }

  /** @override */
  update(notificationId, options, callback) {
    if (this.systemNotificationEnabled) {
      chrome.notifications.update(notificationId, options, callback);
    }
  }

  /** @override */
  setSystemNotificationEnabled(enabled) {
    this.systemNotificationEnabled = enabled;
  }
}

export const notifications = new NotificationsBrowserProxyImpl(false);

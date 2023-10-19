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
   * @param {function(boolean):void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  clear(notificationId, callback) {}

  /**
   * @param {string} notificationId
   * @param {NotificationOptions} options
   * @param {function(string):void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  create(notificationId, options, callback) {}

  /**
   * @param {function(Object):void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  getAll(callback) {}

  /**
   * @param {function(string):void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  getPermissionLevel(callback) {}

  /**
   * @param {string} notificationId
   * @param {NotificationOptions} options
   * @param {function(boolean):void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  update(notificationId, options, callback) {}

  /**
   * @param {boolean} enabled
   */
  // @ts-ignore: error TS6133: 'enabled' is declared but its value is never
  // read.
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
  // @ts-ignore: error TS7006: Parameter 'systemNotificationEnabled' implicitly
  // has an 'any' type.
  constructor(eventType, systemNotificationEnabled) {
    this.eventType = eventType;
    this.systemNotificationEnabled = systemNotificationEnabled;
  }

  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  addListener(callback) {
    if (this.systemNotificationEnabled) {
      switch (this.eventType) {
        case NotificationEventTypes.CLICKED:
          // @ts-ignore: error TS2339: Property 'notifications' does not exist
          // on type 'typeof chrome'.
          chrome.notifications.onClicked.addListener(callback);
          break;
        case NotificationEventTypes.BUTTON_CLICKED:
          // @ts-ignore: error TS2339: Property 'notifications' does not exist
          // on type 'typeof chrome'.
          chrome.notifications.onButtonClicked.addListener(callback);
          break;
        case NotificationEventTypes.CLOSED:
          // @ts-ignore: error TS2339: Property 'notifications' does not exist
          // on type 'typeof chrome'.
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
  // @ts-ignore: error TS7006: Parameter 'systemNotificationEnabled' implicitly
  // has an 'any' type.
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
  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  clear(notificationId, callback) {
    if (this.systemNotificationEnabled) {
      // @ts-ignore: error TS2339: Property 'notifications' does not exist on
      // type 'typeof chrome'.
      chrome.notifications.clear(notificationId, callback);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  create(notificationId, options, callback) {
    if (this.systemNotificationEnabled) {
      // @ts-ignore: error TS2339: Property 'notifications' does not exist on
      // type 'typeof chrome'.
      chrome.notifications.create(notificationId, options, callback);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  getAll(callback) {
    if (this.systemNotificationEnabled) {
      // @ts-ignore: error TS2339: Property 'notifications' does not exist on
      // type 'typeof chrome'.
      chrome.notifications.getAll(callback);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  getPermissionLevel(callback) {
    if (this.systemNotificationEnabled) {
      // @ts-ignore: error TS2339: Property 'notifications' does not exist on
      // type 'typeof chrome'.
      chrome.notifications.getPermissionLevel(callback);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
  // type.
  update(notificationId, options, callback) {
    if (this.systemNotificationEnabled) {
      // @ts-ignore: error TS2339: Property 'notifications' does not exist on
      // type 'typeof chrome'.
      chrome.notifications.update(notificationId, options, callback);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'enabled' implicitly has an 'any' type.
  setSystemNotificationEnabled(enabled) {
    this.systemNotificationEnabled = enabled;
  }
}

export const notifications = new NotificationsBrowserProxyImpl(false);

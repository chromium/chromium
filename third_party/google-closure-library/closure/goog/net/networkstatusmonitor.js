/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Base class for objects monitoring and exposing runtime
 * network status information.
 */

goog.provide('goog.net.NetworkStatusMonitor');

goog.require('goog.events.Listenable');



/**
 * Base class for network status information providers.
 * @interface
 * @extends {goog.events.Listenable}
 */
goog.net.NetworkStatusMonitor = function() {};


/**
 * Enum for the events dispatched by the OnlineHandler.
 * @enum {string}
 */
goog.net.NetworkStatusMonitor.EventType = {
  ONLINE: 'online',
  OFFLINE: 'offline',
};


/**
 * @return {boolean} Whether the system is online or otherwise.
 */
goog.net.NetworkStatusMonitor.prototype.isOnline;

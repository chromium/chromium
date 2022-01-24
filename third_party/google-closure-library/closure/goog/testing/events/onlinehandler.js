/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview NetworkStatusMonitor test double.
 */

goog.setTestOnly('goog.testing.events.OnlineHandler');
goog.provide('goog.testing.events.OnlineHandler');

goog.require('goog.events.EventTarget');
goog.require('goog.net.NetworkStatusMonitor');



/**
 * NetworkStatusMonitor test double.
 * @param {boolean} initialState The initial online state of the mock.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @implements {goog.net.NetworkStatusMonitor}
 * @final
 */
goog.testing.events.OnlineHandler = function(initialState) {
  'use strict';
  goog.testing.events.OnlineHandler.base(this, 'constructor');

  /**
   * Whether the mock is online.
   * @private {boolean}
   */
  this.online_ = initialState;
};
goog.inherits(goog.testing.events.OnlineHandler, goog.events.EventTarget);


/** @override */
goog.testing.events.OnlineHandler.prototype.isOnline = function() {
  'use strict';
  return this.online_;
};


/**
 * Sets the online state.
 * @param {boolean} newOnlineState The new online state.
 */
goog.testing.events.OnlineHandler.prototype.setOnline = function(
    newOnlineState) {
  'use strict';
  if (newOnlineState != this.online_) {
    this.online_ = newOnlineState;
    this.dispatchEvent(
        newOnlineState ? goog.net.NetworkStatusMonitor.EventType.ONLINE :
                         goog.net.NetworkStatusMonitor.EventType.OFFLINE);
  }
};

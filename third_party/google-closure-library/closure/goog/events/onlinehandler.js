/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview This event handler will dispatch events when
 * `navigator.onLine` changes.  HTML5 defines two events, online and
 * offline that is fired on the window.  We listen to the 'online'
 * and 'offline' events on the current window object.
 *
 * Note that this class only reflects what the browser tells us and this usually
 * only reflects whether the browser is connected to the local network.
 *
 * @see ../demos/onlinehandler.html
 */

goog.provide('goog.events.OnlineHandler');
goog.provide('goog.events.OnlineHandler.EventType');

goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.net.NetworkStatusMonitor');



/**
 * Basic object for detecting whether the online state changes.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @implements {goog.net.NetworkStatusMonitor}
 */
goog.events.OnlineHandler = function() {
  'use strict';
  goog.events.OnlineHandler.base(this, 'constructor');

  /**
   * @private {goog.events.EventHandler<!goog.events.OnlineHandler>}
   */
  this.eventHandler_ = new goog.events.EventHandler(this);

  this.eventHandler_.listen(
      window, [goog.events.EventType.ONLINE, goog.events.EventType.OFFLINE],
      this.handleChange_);
};
goog.inherits(goog.events.OnlineHandler, goog.events.EventTarget);


/**
 * Enum for the events dispatched by the OnlineHandler.
 * @enum {string}
 * @deprecated Use goog.net.NetworkStatusMonitor.EventType instead.
 */
goog.events.OnlineHandler.EventType = goog.net.NetworkStatusMonitor.EventType;


/** @override */
goog.events.OnlineHandler.prototype.isOnline = function() {
  'use strict';
  return navigator.onLine;
};


/**
 * Called when the online state changes.  This dispatches the
 * `ONLINE` and `OFFLINE` events respectively.
 * @private
 */
goog.events.OnlineHandler.prototype.handleChange_ = function() {
  'use strict';
  var type = this.isOnline() ? goog.net.NetworkStatusMonitor.EventType.ONLINE :
                               goog.net.NetworkStatusMonitor.EventType.OFFLINE;
  this.dispatchEvent(type);
};


/** @override */
goog.events.OnlineHandler.prototype.disposeInternal = function() {
  'use strict';
  goog.events.OnlineHandler.base(this, 'disposeInternal');
  this.eventHandler_.dispose();
  this.eventHandler_ = null;
};

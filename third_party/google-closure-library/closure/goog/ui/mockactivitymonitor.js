/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition of goog.ui.MockActivityMonitor.
 */

goog.provide('goog.ui.MockActivityMonitor');

goog.require('goog.events.EventType');
goog.require('goog.ui.ActivityMonitor');



/**
 * A mock implementation of goog.ui.ActivityMonitor for unit testing. Clients
 * of this class should override Date.now to return a synthetic time from
 * the unit test.
 * @constructor
 * @extends {goog.ui.ActivityMonitor}
 * @final
 */
goog.ui.MockActivityMonitor = function() {
  'use strict';
  goog.ui.MockActivityMonitor.base(this, 'constructor');

  /**
   * Tracks whether an event has been fired. Used by simulateEvent.
   * @type {boolean}
   * @private
   */
  this.eventFired_ = false;
};
goog.inherits(goog.ui.MockActivityMonitor, goog.ui.ActivityMonitor);


/**
 * Simulates an event that updates the user to being non-idle.
 * @param {goog.events.EventType=} opt_type The type of event that made the user
 *     not idle. If not specified, defaults to MOUSEMOVE.
 */
goog.ui.MockActivityMonitor.prototype.simulateEvent = function(opt_type) {
  'use strict';
  var eventTime = Date.now();
  var eventType = opt_type || goog.events.EventType.MOUSEMOVE;

  this.eventFired_ = false;
  this.updateIdleTime(eventTime, eventType);

  if (!this.eventFired_) {
    this.dispatchEvent(goog.ui.ActivityMonitor.Event.ACTIVITY);
  }
};


/**
 * @override
 */
goog.ui.MockActivityMonitor.prototype.dispatchEvent = function(e) {
  'use strict';
  var rv = goog.ui.MockActivityMonitor.base(this, 'dispatchEvent', e);
  this.eventFired_ = true;
  return rv;
};

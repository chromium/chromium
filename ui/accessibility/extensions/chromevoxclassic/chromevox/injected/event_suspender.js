// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Allows events to be suspended.
 *
 */

goog.provide('cvox.ChromeVoxEventSuspender');

/**
 * @namespace
 */
cvox.ChromeVoxEventSuspender = function() {};

/**
 * A nestable variable to keep track of whether events are suspended.
 *
 * @type {number}
 * @private
 */
cvox.ChromeVoxEventSuspender.suspendLevel_ = 0;

/**
 * Enters a (nested) suspended state.
 */
cvox.ChromeVoxEventSuspender.enterSuspendEvents = function() {
  cvox.ChromeVoxEventSuspender.suspendLevel_ += 1;
}

/**
 * Exits a (nested) suspended state.
 */
cvox.ChromeVoxEventSuspender.exitSuspendEvents = function() {
  cvox.ChromeVoxEventSuspender.suspendLevel_ -= 1;
}

/**
 * Returns true if events are currently suspended.
 *
 * @return {boolean} True if events are suspended.
 */
cvox.ChromeVoxEventSuspender.areEventsSuspended = function() {
  return cvox.ChromeVoxEventSuspender.suspendLevel_ > 0;
};

/**
 * Returns a function that runs the argument with all events suspended.
 *
 * @param {Function} f Function to run with suspended events.
 * @return {?} Returns a function that wraps f.
 */
cvox.ChromeVoxEventSuspender.withSuspendedEvents = function(f) {
  return function() {
    cvox.ChromeVoxEventSuspender.enterSuspendEvents();
    var ret = f.apply(this, arguments);
    cvox.ChromeVoxEventSuspender.exitSuspendEvents();
    return ret;
  };
};

/**
 * Returns a handler that only runs the argument if events are not suspended.
 *
 * @param {Function} handler Function that will be used as an event handler.
 * @param {boolean} ret Return value if events are suspended.
 * @return {Function} Function wrapping the handler.
 */
cvox.ChromeVoxEventSuspender.makeSuspendableHandler = function(handler, ret) {
  return function() {
    if (cvox.ChromeVoxEventSuspender.areEventsSuspended()) {
      return ret;
    }
    return handler();
  };
};

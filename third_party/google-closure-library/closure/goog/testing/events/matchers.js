/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock matchers for event related arguments.
 */

goog.setTestOnly('goog.testing.events.EventMatcher');
goog.provide('goog.testing.events.EventMatcher');

goog.require('goog.events.Event');
goog.require('goog.testing.mockmatchers.ArgumentMatcher');



/**
 * A matcher that verifies that an argument is a `goog.events.Event` of a
 * particular type.
 * @param {string} type The single type the event argument must be of.
 * @constructor
 * @extends {goog.testing.mockmatchers.ArgumentMatcher}
 * @final
 */
goog.testing.events.EventMatcher = function(type) {
  'use strict';
  goog.testing.mockmatchers.ArgumentMatcher.call(this, function(obj) {
    'use strict';
    return obj instanceof goog.events.Event && obj.type == type;
  }, 'isEventOfType(' + type + ')');
};
goog.inherits(
    goog.testing.events.EventMatcher,
    goog.testing.mockmatchers.ArgumentMatcher);

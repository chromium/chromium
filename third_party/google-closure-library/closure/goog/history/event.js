/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The event object dispatched when the history changes.
 */


goog.provide('goog.history.Event');

goog.require('goog.events.Event');
goog.require('goog.history.EventType');



/**
 * Event object dispatched after the history state has changed.
 * @param {string} token The string identifying the new history state.
 * @param {boolean} isNavigation True if the event was triggered by a browser
 *     action, such as forward or back, clicking on a link, editing the URL, or
 *     calling {@code window.history.(go|back|forward)}.
 *     False if the token has been changed by a `setToken` or
 *     `replaceToken` call.
 * @constructor
 * @extends {goog.events.Event}
 * @final
 */
goog.history.Event = function(token, isNavigation) {
  'use strict';
  goog.events.Event.call(this, goog.history.EventType.NAVIGATE);

  /**
   * The current history state.
   * @type {string}
   */
  this.token = token;

  /**
   * Whether the event was triggered by browser navigation.
   * @type {boolean}
   */
  this.isNavigation = isNavigation;
};
goog.inherits(goog.history.Event, goog.events.Event);

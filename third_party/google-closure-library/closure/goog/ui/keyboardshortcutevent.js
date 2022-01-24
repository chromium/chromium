/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The event type emitted by the KeyboardShortcutHandler.
 */
goog.provide('goog.ui.KeyboardShortcutEvent');

goog.require('goog.events.Event');
goog.require('goog.events.EventTarget');

/**
 * Object representing a keyboard shortcut event.
 * @param {string} type Event type.
 * @param {string} identifier Task identifier for the triggered shortcut.
 * @param {Node|goog.events.EventTarget} target Target the original key press
 *     event originated from.
 * @extends {goog.events.Event}
 * @constructor
 * @final
 */
goog.ui.KeyboardShortcutEvent = function(type, identifier, target) {
  'use strict';
  goog.events.Event.call(this, type, target);

  /**
   * Task identifier for the triggered shortcut
   * @type {string}
   */
  this.identifier = identifier;
};
goog.inherits(goog.ui.KeyboardShortcutEvent, goog.events.Event);

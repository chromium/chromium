/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Event type for PageVisibilityMonitor.
 * @see http://www.w3.org/TR/page-visibility/
 */

goog.module('goog.labs.dom.PageVisibilityEvent');
goog.module.declareLegacyNamespace();

const Event = goog.require('goog.events.Event');
const EventType = goog.require('goog.events.EventType');
const PageVisibilityState = goog.require('goog.labs.dom.PageVisibilityState');

/**
 * A page visibility change event.
 * @final
 */
exports = class PageVisibilityEvent extends Event {
  /**
   * Constructs a new PageVisibilityEvent.
   * @param {boolean} hidden Whether the page is hidden.
   * @param {!PageVisibilityState} visibilityState A more detailed visibility
   *     state.
   */
  constructor(hidden, visibilityState) {
    super(EventType.VISIBILITYCHANGE);

    /**
     * Whether the page is hidden.
     * @type {boolean}
     */
    this.hidden = hidden;

    /**
     * A more detailed visibility state.
     * @type {!PageVisibilityState}
     */
    this.visibilityState = visibilityState;
  }
};

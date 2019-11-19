// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview EventTracker is a simple class that manages the addition and
 * removal of DOM event listeners. In particular, it keeps track of all
 * listeners that have been added and makes it easy to remove some or all of
 * them without requiring all the information again. This is particularly handy
 * when the listener is a generated function such as a lambda or the result of
 * calling Function.bind.
 */

// eslint-disable-next-line no-var
/* #export */ var EventTracker = class {
  /**
   * Create an EventTracker to track a set of events.
   * EventTracker instances are typically tied 1:1 with other objects or
   * DOM elements whose listeners should be removed when the object is disposed
   * or the corresponding elements are removed from the DOM.
   */
  constructor() {
    /**
     * @type {Array<EventTracker.Entry>}
     * @private
     */
    this.listeners_ = [];
  }

  /**
   * Add an event listener - replacement for EventTarget.addEventListener.
   * @param {!EventTarget} target The DOM target to add a listener to.
   * @param {string} eventType The type of event to subscribe to.
   * @param {EventListener|Function} listener The listener to add.
   * @param {boolean=} opt_capture Whether to invoke during the capture phase.
   */
  add(target, eventType, listener, opt_capture) {
    const capture = !!opt_capture;
    const h = {
      target: target,
      eventType: eventType,
      listener: listener,
      capture: capture,
    };
    this.listeners_.push(h);
    target.addEventListener(eventType, listener, capture);
  }

  /**
   * Remove any specified event listeners added with this EventTracker.
   * @param {!EventTarget} target The DOM target to remove a listener from.
   * @param {?string} eventType The type of event to remove.
   */
  remove(target, eventType) {
    this.listeners_ = this.listeners_.filter(listener => {
      if (listener.target == target &&
          (!eventType || (listener.eventType == eventType))) {
        EventTracker.removeEventListener(listener);
        return false;
      }
      return true;
    });
  }

  /** Remove all event listeners added with this EventTracker. */
  removeAll() {
    this.listeners_.forEach(
        listener => EventTracker.removeEventListener(listener));
    this.listeners_ = [];
  }

  /**
   * Remove a single event listener given it's tracking entry. It's up to the
   * caller to ensure the entry is removed from listeners_.
   * @param {EventTracker.Entry} entry The entry describing the listener to
   * remove.
   */
  static removeEventListener(entry) {
    entry.target.removeEventListener(
        entry.eventType, entry.listener, entry.capture);
  }
};

/**
 * The type of the internal tracking entry.
 * @typedef {{target: !EventTarget,
 *            eventType: string,
 *            listener: (EventListener|Function),
 *            capture: boolean}}
 */
EventTracker.Entry;

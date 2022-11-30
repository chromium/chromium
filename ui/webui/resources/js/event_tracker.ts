// Copyright 2011 The Chromium Authors
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

export class EventTracker {
  private listeners_: EventTrackerEntry[] = [];

  /**
   * Add an event listener - replacement for EventTarget.addEventListener.
   * @param target The DOM target to add a listener to.
   * @param eventType The type of event to subscribe to.
   * @param listener The listener to add.
   * @param capture Whether to invoke during the capture phase. Defaults to
   *     false.
   */
  add(target: EventTarget, eventType: string,
      listener: EventListener|((p: any) => void), capture: boolean = false) {
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
   * @param target The DOM target to remove a listener from.
   * @param eventType The type of event to remove.
   */
  remove(target: EventTarget, eventType?: string) {
    this.listeners_ = this.listeners_.filter(listener => {
      if (listener.target === target &&
          (!eventType || (listener.eventType === eventType))) {
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
   * @param entry The entry describing the listener to
   * remove.
   */
  static removeEventListener(entry: EventTrackerEntry) {
    entry.target.removeEventListener(
        entry.eventType, entry.listener, entry.capture);
  }
}

// The type of the internal tracking entry.
interface EventTrackerEntry {
  target: EventTarget;
  eventType: string;
  listener: EventListener|(() => void);
  capture: boolean;
}

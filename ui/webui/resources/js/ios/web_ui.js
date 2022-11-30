// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window['chrome'] = window['chrome'] || {};

/**
 * Sends messages to the browser. See
 * https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md#chrome_send
 *
 * @param {string} message name to be passed to the browser.
 * @param {Array=} args optional.
 */
window['chrome']['send'] = function(message, args) {
  __gCrWeb.message.invokeOnHost({
    'command': 'webui.chromeSend',
    'message': message,
    'arguments': args || [],
  });
};


try {
  new EventTarget().constructor;
} catch {
  /**
   * Minimal EventTarget polyfill for webui.
   * TODO(crbug.com/1173902): delete once iOS 13 is deprecated.
   * @constructor
   */
  const EventTarget = function() {
    /**
     * @type {Object}
     */
    this.listeners_ = {};
  };

  /**
   * Registers an event handler of a specific event type on the EventTarget.
   * @param {string} type event type.
   * @param {function} listener event callback.
   * @param {object|boolean|null} options event options.
   */
  EventTarget.prototype.addEventListener = function(type, listener, options) {
    if (!(type in this.listeners_)) {
      this.listeners_[type] = [];
    }
    this.listeners_[type].push({listener: listener, options: options});
  };

  /**
   * Removes an event listener from the EventTarget.
   * @param {string} type event type.
   * @param {function} listener event callback.
   * @param {object|boolean|null} options event options.
   */
  EventTarget.prototype.removeEventListener = function(
      type, listener, options) {
    if (!(type in this.listeners_)) {
      return;
    }

    const stubs = this.listeners_[type];
    let i = 0;
    while (i < stubs.length) {
      if (stubs[i].listener === listener &&
          JSON.stringify(stubs[i].options) === JSON.stringify(options)) {
        stubs.splice(i, 1);
      } else {
        i++;
      }
    }
  };

  /**
   * Dispatches an event to this EventTarget.
   * @param {!Event} event event to dispatch.
   * @return {boolean}
   */
  EventTarget.prototype.dispatchEvent = function(event) {
    if (!(event.type in this.listeners)) {
      return true;
    }

    // Iterate on copy, in case options.once requires removing listener.
    const stubs = this.listeners_[type].slice(0);
    for (let i = 0; i < stubs.length; i++) {
      stubs[i].listener.call(this, event);
      if (typeof stubs[i].options === 'object' && stubs[i].options.once) {
        this.removeEventListener(
            event.type, stubs[i].listener, stubs[i].options);
      }
    }

    return !event.defaultPrevented;
  };

  window.EventTarget = EventTarget;
}

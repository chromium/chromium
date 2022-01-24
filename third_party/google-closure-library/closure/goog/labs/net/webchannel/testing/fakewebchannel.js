/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Implementation of `goog.net.WebChannel` for use in tests. */

goog.module('goog.labs.net.webChannel.testing.FakeWebChannel');
goog.setTestOnly();

const EventTarget = goog.require('goog.events.EventTarget');
const WebChannel = goog.requireType('goog.net.WebChannel');
const {clear} = goog.require('goog.array');
const {fail} = goog.require('goog.testing.asserts');

/**
 * A fake web channel that captures all "sent" messages to memory, for testing.
 * @implements {WebChannel}
 * @final
 */
class FakeWebChannel extends EventTarget {
  constructor() {
    super();

    /** @private {?boolean} */
    this.open_ = null;

    /** @private @const {!Array<!WebChannel.MessageData>} */
    this.messages_ = [];
  }

  /** @override */
  open() {
    this.open_ = true;
  }

  /**
   * @param {!WebChannel.MessageData} messageData
   * @override
   */
  send(messageData) {
    this.messages_.push(messageData);
  }

  /** @override */
  halfClose() {
    fail('Should not be called: not implemented by library');
  }

  /** @override */
  close() {
    this.open_ = false;
  }

  /**
   * @return {!WebChannel.RuntimeProperties}
   * @override
   */
  getRuntimeProperties() {
    return /** @type {!WebChannel.RuntimeProperties} */ ({});
  }

  /**
   * @return {?boolean} whether the channel is open, or `null` if the state has
   *     never changed since initialization.
   */
  isOpen() {
    return this.open_;
  }

  /** Clears the record of sent messages. */
  clearSentMessages() {
    clear(this.messages_);
  }

  /**
   * @return {!Array<!WebChannel.MessageData>} the record of sent messages, in
   *     order of when they were sent.
   */
  getSentMessages() {
    return this.messages_;
  }

  /**
   * @return {!WebChannel.MessageData} the only sent message thus far, if one
   *     and only one such message exists.
   */
  getOnlySentMessage() {
    assertEquals(1, this.messages_.length);
    return this.messages_[0];
  }
}

exports = {FakeWebChannel};

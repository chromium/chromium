/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Functions for manipulating message channels.
 */

goog.provide('goog.messaging');

goog.requireType('goog.messaging.MessageChannel');


/**
 * Creates a bidirectional pipe between two message channels.
 *
 * @param {goog.messaging.MessageChannel} channel1 The first channel.
 * @param {goog.messaging.MessageChannel} channel2 The second channel.
 */
goog.messaging.pipe = function(channel1, channel2) {
  'use strict';
  channel1.registerDefaultService(goog.bind(channel2.send, channel2));
  channel2.registerDefaultService(goog.bind(channel1.send, channel1));
};

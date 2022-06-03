/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Default factory for <code>WebChannelTransport</code> to
 * avoid exposing concrete classes to clients.
 */

goog.provide('goog.net.createWebChannelTransport');

goog.require('goog.labs.net.webChannel.WebChannelBaseTransport');
goog.requireType('goog.net.WebChannelTransport');


/**
 * Create a new WebChannelTransport instance using the default implementation.
 * Throws an error message if no default transport available in the current
 * environment.
 *
 * @return {!goog.net.WebChannelTransport} the newly created transport instance.
 */
goog.net.createWebChannelTransport = function() {
  'use strict';
  return new goog.labs.net.webChannel.WebChannelBaseTransport();
};

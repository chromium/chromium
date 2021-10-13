/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Protocol buffer serializer.
 */

goog.provide('goog.proto');


goog.require('goog.proto.Serializer');


/**
 * Instance of the serializer object.
 * @type {goog.proto.Serializer}
 * @private
 */
goog.proto.serializer_ = null;


/**
 * Serializes an object or a value to a protocol buffer string.
 * @param {Object} object The object to serialize.
 * @return {string} The serialized protocol buffer string.
 */
goog.proto.serialize = function(object) {
  'use strict';
  if (!goog.proto.serializer_) {
    goog.proto.serializer_ = new goog.proto.Serializer;
  }
  return goog.proto.serializer_.serialize(object);
};

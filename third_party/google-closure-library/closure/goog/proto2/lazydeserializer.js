/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Base class for all PB2 lazy deserializer. A lazy deserializer
 *   is a serializer whose deserialization occurs on the fly as data is
 *   requested. In order to use a lazy deserializer, the serialized form
 *   of the data must be an object or array that can be indexed by the tag
 *   number.
 */

goog.provide('goog.proto2.LazyDeserializer');

goog.require('goog.asserts');
goog.require('goog.proto2.Message');
goog.require('goog.proto2.Serializer');
goog.requireType('goog.proto2.FieldDescriptor');



/**
 * Base class for all lazy deserializers.
 *
 * @constructor
 * @extends {goog.proto2.Serializer}
 */
goog.proto2.LazyDeserializer = function() {};
goog.inherits(goog.proto2.LazyDeserializer, goog.proto2.Serializer);


/** @override */
goog.proto2.LazyDeserializer.prototype.deserialize = function(
    descriptor, data) {
  'use strict';
  var message = descriptor.createMessageInstance();
  message.initializeForLazyDeserializer(this, data);
  goog.asserts.assert(message instanceof goog.proto2.Message);
  return message;
};


/** @override */
goog.proto2.LazyDeserializer.prototype.deserializeTo = function(message, data) {
  'use strict';
  throw new Error('Unimplemented');
};


/**
 * Deserializes a message field from the expected format and places the
 * data in the given message
 *
 * @param {goog.proto2.Message} message The message in which to
 *     place the information.
 * @param {goog.proto2.FieldDescriptor} field The field for which to set the
 *     message value.
 * @param {*} data The serialized data for the field.
 *
 * @return {*} The deserialized data or null for no value found.
 */
goog.proto2.LazyDeserializer.prototype.deserializeField = goog.abstractMethod;

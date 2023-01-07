/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Protocol Buffer 2 Serializer which serializes messages
 *  into PB-Lite ("JsPbLite") format.
 *
 * PB-Lite format is an array where each index corresponds to the associated tag
 * number. For example, a message like so:
 *
 * message Foo {
 *   optional int bar = 1;
 *   optional int baz = 2;
 *   optional int bop = 4;
 * }
 *
 * would be represented as such:
 *
 * [, (bar data), (baz data), (nothing), (bop data)]
 *
 * Note that since the array index is used to represent the tag number, sparsely
 * populated messages with tag numbers that are not continuous (and/or are very
 * large) will have many (empty) spots and thus, are inefficient.
 *
 */

goog.provide('goog.proto2.PbLiteSerializer');

goog.require('goog.asserts');
goog.require('goog.proto2.FieldDescriptor');
goog.require('goog.proto2.LazyDeserializer');
goog.require('goog.proto2.Serializer');
goog.requireType('goog.proto2.Message');



/**
 * PB-Lite serializer.
 *
 * @constructor
 * @extends {goog.proto2.LazyDeserializer}
 */
goog.proto2.PbLiteSerializer = function() {};
goog.inherits(goog.proto2.PbLiteSerializer, goog.proto2.LazyDeserializer);


/**
 * If true, fields will be serialized with 0-indexed tags (i.e., the proto
 * field with tag id 1 will have index 0 in the array).
 * @type {boolean}
 * @private
 */
goog.proto2.PbLiteSerializer.prototype.zeroIndexing_ = false;


/**
 * By default, the proto tag with id 1 will have index 1 in the serialized
 * array.
 *
 * If the serializer is set to use zero-indexing, the tag with id 1 will have
 * index 0.
 *
 * @param {boolean} zeroIndexing Whether this serializer should deal with
 *     0-indexed protos.
 */
goog.proto2.PbLiteSerializer.prototype.setZeroIndexed = function(zeroIndexing) {
  'use strict';
  this.zeroIndexing_ = zeroIndexing;
};


/**
 * Serializes a message to a PB-Lite object.
 *
 * @param {goog.proto2.Message} message The message to be serialized.
 * @return {!Array<?>} The serialized form of the message.
 * @override
 */
goog.proto2.PbLiteSerializer.prototype.serialize = function(message) {
  'use strict';
  var descriptor = message.getDescriptor();
  var fields = descriptor.getFields();

  var serialized = [];
  var zeroIndexing = this.zeroIndexing_;

  // Add the known fields.
  for (var i = 0; i < fields.length; i++) {
    var field = fields[i];

    if (!message.has(field)) {
      continue;
    }

    var tag = field.getTag();
    var index = zeroIndexing ? tag - 1 : tag;

    if (field.isRepeated()) {
      serialized[index] = [];

      for (var j = 0; j < message.countOf(field); j++) {
        serialized[index][j] =
            this.getSerializedValue(field, message.get(field, j));
      }
    } else {
      serialized[index] = this.getSerializedValue(field, message.get(field));
    }
  }

  // Add any unknown fields.
  message.forEachUnknown(function(tag, value) {
    'use strict';
    var index = zeroIndexing ? tag - 1 : tag;
    serialized[index] = value;
  });

  return serialized;
};


/** @override */
goog.proto2.PbLiteSerializer.prototype.deserializeField = function(
    message, field, value) {
  'use strict';
  if (value == null) {
    // Since value double-equals null, it may be either null or undefined.
    // Ensure we return the same one, since they have different meanings.
    // TODO(user): If the field is repeated, this method should probably
    // return [] instead of null.
    return value;
  }

  if (field.isRepeated()) {
    var data = [];

    goog.asserts.assert(Array.isArray(value), 'Value must be array: %s', value);

    for (var i = 0; i < value.length; i++) {
      data[i] = this.getDeserializedValue(field, value[i]);
    }

    return data;
  } else {
    return this.getDeserializedValue(field, value);
  }
};


/** @override */
goog.proto2.PbLiteSerializer.prototype.getSerializedValue = function(
    field, value) {
  'use strict';
  if (field.getFieldType() == goog.proto2.FieldDescriptor.FieldType.BOOL) {
    // Booleans are serialized in numeric form.
    return value ? 1 : 0;
  }

  return goog.proto2.Serializer.prototype.getSerializedValue.apply(
      this, arguments);
};


/** @override */
goog.proto2.PbLiteSerializer.prototype.getDeserializedValue = function(
    field, value) {
  'use strict';
  if (field.getFieldType() == goog.proto2.FieldDescriptor.FieldType.BOOL) {
    goog.asserts.assert(
        typeof value === 'number' || typeof value === 'boolean',
        'Value is expected to be a number or boolean');
    return !!value;
  }

  return goog.proto2.Serializer.prototype.getDeserializedValue.apply(
      this, arguments);
};


/** @override */
goog.proto2.PbLiteSerializer.prototype.deserialize = function(
    descriptor, data) {
  'use strict';
  var toConvert = data;
  if (this.zeroIndexing_) {
    // Make the data align with tag-IDs (1-indexed) by shifting everything
    // up one.
    toConvert = [];
    for (var key in data) {
      toConvert[parseInt(key, 10) + 1] = data[key];
    }
  }
  return goog.proto2.PbLiteSerializer.base(
      this, 'deserialize', descriptor, toConvert);
};

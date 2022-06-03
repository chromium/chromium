/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview a fuzzing JSON generator.
 *
 * This class generates a random JSON-compatible array object under the
 * following rules, (n) n being the relative weight of enum/discrete values
 * of a stochastic variable:
 * 1. Total number of elements for the generated JSON array: [1, 10)
 * 2. Each element: with message (1), array (1)
 * 3. Each message: number of fields: [0, 5); field type: with
 *    message (5), string (1), number (1), boolean (1), array (1), null (1)
 * 4. Message may be nested, and will be terminated randomly with
 *    a max depth equal to 5
 * 5. Each array: length [0, 5), and may be nested too
 */

goog.provide('goog.labs.testing.JsonFuzzing');

goog.require('goog.string');
goog.require('goog.testing.PseudoRandom');



/**
 * The JSON fuzzing generator.
 *
 * @param {!goog.labs.testing.JsonFuzzing.Options=} opt_options Configuration
 *     for the fuzzing json generator.
 * @param {number=} opt_seed The seed for the random generator.
 * @constructor
 * @struct
 */
goog.labs.testing.JsonFuzzing = function(opt_options, opt_seed) {
  'use strict';
  /**
   * The config options.
   * @private {!goog.labs.testing.JsonFuzzing.Options}
   */
  this.options_ =
      opt_options || {jsonSize: 10, numFields: 5, arraySize: 5, maxDepth: 5};

  /**
   * The random generator
   * @private {!goog.testing.PseudoRandom}
   */
  this.random_ = new goog.testing.PseudoRandom(opt_seed);

  /**
   * The depth limit, which defaults to 5.
   * @private {number}
   */
  this.maxDepth_ = this.options_.maxDepth;
};


/**
 * Configuration spec.
 *
 * jsonSize: default to [1, 10) for the entire JSON object (array)
 * numFields: default to [0, 5)
 * arraySize: default to [0, 5) for the length of nested arrays
 * maxDepth: default to 5
 *
 * @typedef {{
 *   jsonSize: number,
 *   numFields: number,
 *   arraySize: number,
 *   maxDepth: number
 * }}
 */
goog.labs.testing.JsonFuzzing.Options;


/**
 * Gets a fuzzily-generated JSON object (an array).
 *
 * TODO(user): whitespaces
 *
 * @return {!Array} A new JSON compliant array object.
 */
goog.labs.testing.JsonFuzzing.prototype.newArray = function() {
  'use strict';
  const result = [];
  const depth = 0;

  const maxSize = this.options_.jsonSize;

  const size = this.nextInt(1, maxSize);
  for (let i = 0; i < size; i++) {
    result.push(this.nextElm_(depth));
  }

  return result;
};


/**
 * Gets a new integer.
 *
 * @param {number} min Inclusive
 * @param {number} max Exclusive
 * @return {number} A random integer
 */
goog.labs.testing.JsonFuzzing.prototype.nextInt = function(min, max) {
  'use strict';
  const random = this.random_.random();

  return Math.floor(random * (max - min)) + min;
};


/**
 * Gets a new element type, randomly.
 *
 * @return {number} 0 for message and 1 for array.
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextElmType_ = function() {
  'use strict';
  const random = this.random_.random();

  if (random < 0.5) {
    return 0;
  } else {
    return 1;
  }
};


/**
 * Enum type for the field type (of a message).
 * @enum {number}
 * @private
 */
goog.labs.testing.JsonFuzzing.FieldType_ = {
  /**
   * Message field.
   */
  MESSAGE: 0,

  /**
   * Array field.
   */
  ARRAY: 1,

  /**
   * String field.
   */
  STRING: 2,

  /**
   * Numeric field.
   */
  NUMBER: 3,

  /**
   * Boolean field.
   */
  BOOLEAN: 4,

  /**
   * Null field.
   */
  NULL: 5
};


/**
 * Get a new field type, randomly.
 *
 * @return {!goog.labs.testing.JsonFuzzing.FieldType_} the field type.
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextFieldType_ = function() {
  'use strict';
  const FieldType = goog.labs.testing.JsonFuzzing.FieldType_;

  const random = this.random_.random();

  if (random < 0.5) {
    return FieldType.MESSAGE;
  } else if (random < 0.6) {
    return FieldType.ARRAY;
  } else if (random < 0.7) {
    return FieldType.STRING;
  } else if (random < 0.8) {
    return FieldType.NUMBER;
  } else if (random < 0.9) {
    return FieldType.BOOLEAN;
  } else {
    return FieldType.NULL;
  }
};


/**
 * Gets a new element.
 *
 * @param {number} depth The depth
 * @return {!Object} a random element, msg or array
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextElm_ = function(depth) {
  'use strict';
  switch (this.nextElmType_()) {
    case 0:
      return this.nextMessage_(depth);
    case 1:
      return this.nextArray_(depth);
    default:
      throw new Error('invalid elm type encounted.');
  }
};


/**
 * Gets a new message.
 *
 * @param {number} depth The depth
 * @return {!Object} a random message.
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextMessage_ = function(depth) {
  'use strict';
  if (depth > this.maxDepth_) {
    return {};
  }

  const numFields = this.options_.numFields;

  const random_num = this.nextInt(0, numFields);
  const result = {};

  // TODO(user): unicode and random keys
  for (let i = 0; i < random_num; i++) {
    switch (this.nextFieldType_()) {
      case 0:
        result['f' + i] = this.nextMessage_(depth++);
        continue;
      case 1:
        result['f' + i] = this.nextArray_(depth++);
        continue;
      case 2:
        result['f' + i] = goog.string.getRandomString();
        continue;
      case 3:
        result['f' + i] = this.nextNumber_();
        continue;
      case 4:
        result['f' + i] = this.nextBoolean_();
        continue;
      case 5:
        result['f' + i] = null;
        continue;
      default:
        throw new Error('invalid field type encounted.');
    }
  }

  return result;
};


/**
 * Gets a new array.
 *
 * @param {number} depth The depth
 * @return {!Array} a random array.
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextArray_ = function(depth) {
  'use strict';
  if (depth > this.maxDepth_) {
    return [];
  }

  const size = this.options_.arraySize;

  const random_size = this.nextInt(0, size);
  const result = [];

  // mixed content
  for (let i = 0; i < random_size; i++) {
    switch (this.nextFieldType_()) {
      case 0:
        result.push(this.nextMessage_(depth++));
        continue;
      case 1:
        result.push(this.nextArray_(depth++));
        continue;
      case 2:
        result.push(goog.string.getRandomString());
        continue;
      case 3:
        result.push(this.nextNumber_());
        continue;
      case 4:
        result.push(this.nextBoolean_());
        continue;
      case 5:
        result.push(null);
        continue;
      default:
        throw new Error('invalid field type encounted.');
    }
  }

  return result;
};


/**
 * Gets a new boolean.
 *
 * @return {boolean} a random boolean.
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextBoolean_ = function() {
  'use strict';
  const random = this.random_.random();

  return random < 0.5;
};


/**
 * Gets a new number.
 *
 * @return {number} a random number..
 * @private
 */
goog.labs.testing.JsonFuzzing.prototype.nextNumber_ = function() {
  'use strict';
  let result = this.random_.random();

  let random = this.random_.random();
  if (random < 0.5) {
    result *= 1000;
  }

  random = this.random_.random();
  if (random < 0.5) {
    result = Math.floor(result);
  }

  random = this.random_.random();
  if (random < 0.5) {
    result *= -1;
  }

  // TODO(user); more random numbers

  return result;
};

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the utility function `goog.debug.deepFreeze` to
 * enforce deep immutability of objects as per the style guide, only in
 * non-production builds.
 */
goog.module('goog.debug.deepFreeze');

const {enhanceError, freeze} = goog.require('goog.debug');

/**
 * @private
 */
const throwingGetterError_ = new Error(
    'Retrieving object values after deepFreeze is disallowed. Please use the frozen object instead.');

/**
 * @type {!ObjectPropertyDescriptor}
 * @private
 */
const throwingPropertyDescriptor_ = {
  configurable: false,
  get: function() {
    throw throwingGetterError_;
  },
  set: function() {
    throw new Error(
        'Setting object values after deepFreeze is disallowed. Please use the frozen object instead.');
  },
};

/**
 * Replaces object property accesses with throwing getters to discourage the use
 * of the non-frozen version of an object used with deepFreeze.
 * @param {?Object} arg The object whose accessors should be broken.
 * @private
 */
const deepFreezeBreakObjectInternal_ = function(arg) {
  if (!arg) {
    return;
  }
  switch (typeof arg) {
    case 'object':
      break;
    default:
      return;
  }

  const keys = [
    ...Object.getOwnPropertyNames(arg),
    ...Object.getOwnPropertySymbols(arg),
  ];
  const descriptorBundle = {};
  for (const key of keys) {
    const descriptor = Object.getOwnPropertyDescriptor(arg, key);
    if (!descriptor.enumerable) {
      continue;
    }

    let child;
    try {
      child = arg[key];
    } catch (e) {
      if (e !== throwingGetterError_) {
        // We aren't sure what the error here is. Enhance and return it to
        // callers.
        throw enhanceError(e);
      } else {
        // Here we have already broken this object (via some other path to the
        // object in the parent).
        continue;
      }
    }
    deepFreezeBreakObjectInternal_(child);
    // Batch-overwrite the original arg's value with a throwing getter
    descriptorBundle[key] = throwingPropertyDescriptor_;
  }
  Object.defineProperties(arg, descriptorBundle);
};

/**
 * Deep freezes the given object, but only in debug mode and in browsers that
 * support freezing.
 *
 * @param {T} arg The object to clone.
 * @param {!Set<?>} seenSet The set of objects seen so far while recursing into
 *     child objects. Used to detect cyclic objects.
 * @return {T}
 * @template T
 * @private
 */
const deepFreezeInternal_ = function(arg, seenSet) {
  // Check for primitives and non-recursive object types to avoid adding to seen
  // set.
  switch (typeof arg) {
    case 'function':
      throw new Error('deepFreeze does not support functions');
    case 'object':
      if (arg === null) {
        return null;
      }
      break;
    default:
      // Primitives. Return them as they are effectively immutable.
      return arg;
  }

  if (seenSet.has(arg)) {
    throw new Error('deepFreeze does not support cyclic structures');
  }

  // Check and see if the arg is either an Array literal or an Object literal
  // (e.g isn't a class).
  const prototype = Object.getPrototypeOf(arg);
  if (prototype !== Object.prototype && prototype !== Array.prototype) {
    throw new Error('deepFreeze only supports literals (array or object).');
  }

  seenSet.add(arg);
  const dupe = prototype === Array.prototype ? new Array(arg.length) : {};
  const keys = [
    ...Object.getOwnPropertyNames(arg),
    ...Object.getOwnPropertySymbols(arg),
  ];

  for (const key of keys) {
    const descriptor = Object.getOwnPropertyDescriptor(arg, key);
    if (!descriptor.enumerable) {
      continue;
    }
    if (descriptor.get != null || descriptor.set != null) {
      throw new Error('deepFreeze does not support getters/setters');
    }
    const frozen = deepFreezeInternal_(arg[key], seenSet);
    dupe[key] = frozen;
  }

  seenSet.delete(arg);
  freeze(dupe);
  return dupe;
};

/**
 * Deep-freezes the given object, but only in debug mode (and in browsers
 * that support it). This freeze is deep, and will automatically recurse
 * into object properties and freeze them. This implementation may return a copy
 * of the original object, and the return value should be used instead of the
 * original argument. This implementation only supports literal object
 * structures, and does not attempt to freeze classes, functions, etc.
 * @param {T} arg
 * @return {T}
 * @template T
 */
const deepFreeze = function(arg) {
  // NOTE: this compiles to nothing, but hides the possible side effect of
  // deepFreezeInternal_ from the compiler so that the entire call can be
  // removed if the result is not used.
  return {
    valueOf: function() {
      if (!goog.DEBUG) return arg;
      const dupe = deepFreezeInternal_(arg, new Set());
      deepFreezeBreakObjectInternal_(arg);
      return dupe;
    },
  }.valueOf();
};

exports = {deepFreeze};

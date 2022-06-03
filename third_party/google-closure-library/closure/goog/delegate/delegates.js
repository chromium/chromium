/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides some utility methods for calling delegate lists with
 * common "calling conventions".
 *
 * @see goog.delegate.DelegateRegistry
 */

goog.module('goog.delegate.delegates');


/**
 * Calls the first delegate, or returns undefined if none are given.
 * @param {!Array<T>} delegates
 * @param {function(T): R} mapper
 * @param {function(): (R|undefined)=} defaultImpl
 * @return {R|undefined}
 * @template T, R
 */
exports.callFirst = (delegates, mapper, defaultImpl = undefined) => {
  if (delegates.length === 0) {
    return defaultImpl ? defaultImpl() : undefined;
  }
  return mapper(delegates[0]);
};


/**
 * Calls delegates until one returns a defined, non-null result.  Returns
 * undefined if no such element is found.
 * @param {!Array<T>} delegates
 * @param {function(T): R|undefined} mapper
 * @param {function(): (R|undefined)=} defaultImpl
 * @return {R|undefined}
 * @template T, R
 */
exports.callUntilDefinedAndNotNull =
    (delegates, mapper, defaultImpl = undefined) => {
      if (delegates.length === 0) {
        return defaultImpl ? defaultImpl() : undefined;
      }
      for (const delegate of delegates) {
        const result = mapper(delegate);
        if (result != null) return result;
      }
      return undefined;
    };


/**
 * Calls delegates until one returns a truthy result.  Returns false if no such
 * element is found.
 * @param {!Array<T>} delegates
 * @param {function(T): R} mapper
 * @param {function(): (R|boolean)=} defaultImpl
 * @return {boolean|R}
 * @template T, R
 */
exports.callUntilTruthy = (delegates, mapper, defaultImpl = undefined) => {
  if (delegates.length === 0) {
    return defaultImpl ? defaultImpl() : false;
  }
  for (const delegate of delegates) {
    const result = mapper(delegate);
    if (result) return result;
  }
  return false;
};

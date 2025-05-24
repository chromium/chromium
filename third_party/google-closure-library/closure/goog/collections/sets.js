/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Set operations for ES6 Sets.
 *
 * See design doc at go/closure-es6-set
 */

goog.module('goog.collections.sets');

const iters = goog.require('goog.collections.iters');

// Note: Set operations are being proposed for EcmaScript. See proposal here:
// https://github.com/tc39/proposal-set-methods

// When these methods become available in JS engines, they should be used in
// place of these utility methods and these methods will be deprecated.
// Call sites can be automatically migrated. For example,
// "iters.filter(a, b)" becomes "a.filter(b)".

/**
 * A SetLike implements the same public interface as an ES6 Set, without tying
 * the underlying code directly to the implementation. Any additions to this
 * type should also be present on ES6 Sets.
 * @template T
 * @extends {Iterable<T>}
 * @record
 */
class SetLike {
  constructor() {
    /** @const {number} The number of items in this set. */
    this.size;
  }

  /**
   * @param {T} val The value to add to the Set.
   */
  add(val) {};

  /**
   * @param {T} val The value to remove from the Set.
   * @return {boolean} Whether the value was removed from the set.
   */
  delete(val) {};

  /**
   * @param {T} val The value to check.
   * @return {boolean} True iff this value is present in the set.
   */
  has(val) {};
}
exports.SetLike = SetLike;

/**
 * Creates a new ES6 Set containing the elements that appear in both given
 * collections.
 *
 * @param {!SetLike<T>} a
 * @param {!Iterable<T>} b
 * @returns {!Set<T>}
 * @template T
 */
exports.intersection = function(a, b) {
  return new Set(iters.filter(b, elem => a.has(elem)));
};

/**
 * Creates a new ES6 Set containing the elements that appear in both given
 * collections.
 *
 * @param {!SetLike<T>} a
 * @param {!Iterable<T>} b
 * @return {!Set<T>}
 * @template T
 */
exports.union = function(a, b) {
  const set = new Set(a);
  iters.forEach(b[Symbol.iterator](), elem => set.add(elem));
  return set;
};


/**
 * Creates a new ES6 Set containing the elements that appear in the first
 * collection but not in the second.
 *
 * @param {!SetLike<T>} a
 * @param {!Iterable<T>} b
 * @return {!Set<T>}
 * @template T
 */
exports.difference = function(a, b) {
  const set = new Set(a);
  iters.forEach(b[Symbol.iterator](), elem => set.delete(elem));
  return set;
};

/**
 * Creates a new set containing the elements that appear in a or b but not
 * both.
 *
 * @param {!Set<T>} a
 * @param {!Set<T>} b
 * @return {!Set<T>}
 * @template T
 */
// TODO(nnaze): Consider widening the type of b per discussion in
// https://github.com/tc39/proposal-set-methods/issues/56
exports.symmetricDifference = function(a, b) {
  const newSet = new Set(a);
  for (const elem of b) {
    if (a.has(elem)) {
      newSet.delete(elem);
    } else {
      newSet.add(elem);
    }
  }
  return newSet;
};

/**
 * Adds all the values in the given iterable to the given set.
 * @param {!SetLike<T>} set The set to add items to.
 * @param {!Iterable<T>} col A collection containing items to add.
 * @template T
 */
exports.addAll = function(set, col) {
  for (const elem of col) {
    set.add(elem);
  }
};

/**
 * Removes all values in the given collection from the given set.
 * @param {!SetLike<T>} set The set to remove items from.
 * @param {!Iterable<T>} col A collection containing the elements to remove.
 * @template T
 */
exports.removeAll = function(set, col) {
  for (const elem of col) {
    set.delete(elem);
  }
};

/**
 * Checks the given set contains all members of the given collection.
 * @param {!SetLike<T>} set The set to check for item presence.
 * @param {!Iterable<T>} col The collection of items to check for.
 * @return {boolean} True iff the given set contains all the elements in the
 *     given collection, false otherwise.
 * @template T
 */
exports.hasAll = function(set, col) {
  for (const elem of col) {
    if (!set.has(elem)) return false;
  }
  return true;
};

/**
 * Tests whether the given collection consists of the same elements as the
 * given set, regardless of order, without repetition. This operation is O(n).
 * @param {!SetLike<T>} set The first set which might be equal to the given
 *     collection.
 * @param {!SetLike<T>|!Array<T>} col The second collection of items.
 * @return {boolean} True iff the given collections are equal (contain) contains
 *     all the elements in the given collection, false otherwise.
 * @template T
 */
exports.equals = function(set, col) {
  const colSize = Array.isArray(col) ? col.length : col.size;
  if (set.size !== colSize) {
    return false;
  }
  return exports.isSubsetOf(set, col);
};

/**
 * Tests whether all elements in the set are contained in the given collection.
 * This operation is O(n).
 * @param {!SetLike<T>} set The set which might be a subset of the given
 *     collection.
 * @param {!SetLike<T>|!Array<T>} col The second collection of items.
 * @return {boolean} True iff set A is a subset of collection B, false
 *     otherwise.
 * @template T
 */
exports.isSubsetOf = function(set, col) {
  if (Array.isArray(col) && set.size > col.length) return false;
  const colSet = Array.isArray(col) ? new Set(col) : col;
  if (set.size > colSet.size) {
    return false;
  }
  for (const elem of set) {
    if (!colSet.has(elem)) return false;
  }
  return true;
};

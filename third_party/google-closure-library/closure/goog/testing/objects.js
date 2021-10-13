/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Testing utilities for generic objects.
 */

goog.module('goog.testing.objects');
goog.setTestOnly('goog.testing.objects');

const asserts = goog.require('goog.testing.asserts');

/**
 * Asserts that the given object has a transitive reference on the given value.
 *
 * This may be useful when writing tests to confirm that certain values'leaked'.
 */
function assertRetainsReference(/** !Object */ object, /** * */ value) {
  const path = searchForReference(object, value);
  if (!path) {
    asserts.raiseException(
        `The object does not transitively retain a reference to the given value`);
  }
}

/**
 * Asserts that the given object has no transitive reference on the given
 * value.
 *
 * This may be useful when writing tests to confirm that certain values aren't
 * 'leaked'.
 */
function assertDoesNotRetainReference(/** !Object */ object, /** * */ value) {
  const path = searchForReference(object, value);
  if (path) {
    asserts.raiseException(
        `expected there to be no retention path, found the value @ object['${
            path.join('\'][\'')}']`);
  }
}

/**
 * Searches an object for a value and returns the path to it, or `null` if it
 * cannot be found.
 *
 * @param {!Object} object The object to search, recursively
 * @param {?} needle The value to search for
 * @return {?Array<string>} the path to the value, or `null` if there is no such
 *     path
 */
function searchForReference(object, needle) {
  const /** !Set<!Object> */ visited = new Set();
  const /** !Array<string> */ stack = [];
  /** @return {boolean} */
  const doSearch = (/** !Object */ object, /** ? */ needle) => {
    if (object === needle) {
      return true;
    }
    if (!object || visited.has(object)) {
      return false;  // cycle or null
    }
    visited.add(object);
    for (const key in object) {
      stack.push(key);
      if (doSearch(object[key], needle)) {
        return true;
      }
      stack.pop();
    }
    return false;
  };
  if (doSearch(object, needle)) {
    return stack;
  }
  return null;
}

exports = {
  assertDoesNotRetainReference,
  assertRetainsReference,
};

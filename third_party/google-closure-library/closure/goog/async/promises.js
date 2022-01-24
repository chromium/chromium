/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides utility functions for promises.
 */

goog.module('goog.async.promises');


/**
 * Resolves when all promise values in the map resolve. The resolved value will
 * be a Map with the same keys as the input map, but with the resolved values of
 * the promises. Rejects with first error if any promise rejects. Like
 * Promise.all(), but for Maps.
 *
 * @template OUT_VALUE :=
 *    mapunion(IN_VALUE, (V) =>
 *      cond(isTemplatized(V) && sub(rawTypeOf(V), 'IThenable'),
 *        templateTypeOf(V, 0),
 *          cond(sub(V, 'Thenable'),
 *            unknown(),
 *            V)))
 * =:
 * @template KEY
 * @template IN_VALUE
 *
 * @param {!Map<KEY, IN_VALUE>} promiseMap
 * @return {!Promise<!Map<KEY, OUT_VALUE>>}
 */
exports.allMapValues = (promiseMap) => {
  // Maps return keys and values in insertion order, so these will match each
  // other.
  const keys = Array.from(promiseMap.keys());
  const values = Array.from(promiseMap.values());
  return Promise.all(values).then((results) => {
    const resultMap = new Map();
    results.forEach((result, i) => resultMap.set(keys[i], result));
    return resultMap;
  });
};

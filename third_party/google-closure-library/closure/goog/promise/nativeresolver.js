/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.promise.NativeResolver');

/**
 * Creates a new JavaScript native Promise and captures its resolve and reject
 * callbacks. The promise, resolve, and reject are available as properties
 * @final
 * @template T
 */
class NativeResolver {
  constructor() {
    /** @type {function((T|!IThenable<T>|!Thenable)=)} */
    this.resolve;
    /** @type {function(*=)} */
    this.reject;

    /** @type {!Promise<T>} */
    this.promise = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
}

exports = NativeResolver;

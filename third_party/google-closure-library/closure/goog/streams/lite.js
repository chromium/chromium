/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A lite polyfill of the ReadableStream native API with a subset
 * of methods supported.
 */
goog.module('goog.streams.lite');

const liteImpl = goog.require('goog.streams.liteImpl');
const liteNativeImpl = goog.require('goog.streams.liteNativeImpl');
const {ReadableStream, ReadableStreamDefaultController, ReadableStreamDefaultReader, ReadableStreamUnderlyingSource} = goog.require('goog.streams.liteTypes');
const {USE_NATIVE_IMPLEMENTATION} = goog.require('goog.streams.defines');

/**
 * Creates and returns a new ReadableStream.
 *
 * The underlying source should only have a start() method, and no other
 * properties.
 * @param {!ReadableStreamUnderlyingSource<T>} underlyingSource
 * @return {!ReadableStream<T>}
 * @suppress {strictMissingProperties}
 * @template T
 */
function newReadableStream(underlyingSource) {
  if (USE_NATIVE_IMPLEMENTATION === 'true' ||
      (USE_NATIVE_IMPLEMENTATION === 'detect' && goog.global.ReadableStream)) {
    return liteNativeImpl.newReadableStream(underlyingSource);
  } else {
    return liteImpl.newReadableStream(underlyingSource);
  }
}

exports = {
  ReadableStream,
  ReadableStreamDefaultController,
  ReadableStreamDefaultReader,
  ReadableStreamUnderlyingSource,
  newReadableStream,
};

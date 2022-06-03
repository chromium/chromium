/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A full ponyfill of the ReadableStream native API.
 */
goog.module('goog.streams.full');

const fullImpl = goog.require('goog.streams.fullImpl');
const fullNativeImpl = goog.require('goog.streams.fullNativeImpl');
const {ReadableStream, ReadableStreamAsyncIterator, ReadableStreamDefaultController, ReadableStreamDefaultReader, ReadableStreamStrategy, ReadableStreamUnderlyingSource} = goog.require('goog.streams.fullTypes');
const {USE_NATIVE_IMPLEMENTATION} = goog.require('goog.streams.defines');

/**
 * Creates and returns a new ReadableStream.
 *
 * The underlying source should only have a start() method, and no other
 * properties.
 * @param {!ReadableStreamUnderlyingSource<T>=} underlyingSource
 * @param {!ReadableStreamStrategy<T>=} strategy
 * @return {!ReadableStream<T>}
 * @suppress {strictMissingProperties}
 * @template T
 */
function newReadableStream(underlyingSource = {}, strategy = {}) {
  if (USE_NATIVE_IMPLEMENTATION === 'true' ||
      (USE_NATIVE_IMPLEMENTATION === 'detect' && goog.global.ReadableStream)) {
    return fullNativeImpl.newReadableStream(underlyingSource, strategy);
  } else {
    return fullImpl.newReadableStream(underlyingSource, strategy);
  }
}

exports = {
  ReadableStream,
  ReadableStreamAsyncIterator,
  ReadableStreamDefaultController,
  ReadableStreamDefaultReader,
  ReadableStreamStrategy,
  ReadableStreamUnderlyingSource,
  newReadableStream,
};

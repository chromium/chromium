/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Types provided by the lite implementation. DO NOT WRITE
 * IMPLEMANTATIONS OF THE INTERFACES PROVIDED HERE. These exist to provide
 * a super type for the native-wrapped impl and the ponyfill impl.
 */
goog.module('goog.streams.liteTypes');

/**
 * The lite ReadableStream.
 *
 * Supports the getReader() method and locked property.
 *
 * The only method of underlying sources that is supported is enqueueing,
 * closing, and erroring.
 *
 * Pulling (including backpressure and sizes) and cancellation are not
 * supported.
 * @template T
 * @interface
 */
class ReadableStream {
  /**
   * Returns true if the ReadableStream has been locked to a reader.
   * https://streams.spec.whatwg.org/#rs-locked
   * @return {boolean}
   */
  get locked() {}

  /**
   * Returns a ReadableStreamDefaultReader that enables reading chunks from
   * the source.
   * https://streams.spec.whatwg.org/#rs-get-reader
   * @return {!ReadableStreamDefaultReader<T>}
   */
  getReader() {}
}

/**
 * A reader for a lite ReadableStream.
 *
 * Supports the read() and releaseLock() methods, along with the closed
 * property.
 * @template T
 * @interface
 */
class ReadableStreamDefaultReader {
  /**
   * Returns a Promise that resolves when the Stream closes or is errored, or if
   * the reader releases its lock.
   * https://streams.spec.whatwg.org/#default-reader-closed
   * @return {!Promise<undefined>}
   */
  get closed() {}

  /**
   * Returns a Promise that resolves with an IIterableResult providing the next
   * chunk or that the stream is closed. The Promise may reject if the stream
   * is errored.
   * https://streams.spec.whatwg.org/#default-reader-read
   * @return {!Promise<!IIterableResult<T>>}
   */
  read() {}

  /**
   * Release the lock on the stream. Any further calls to read() will error,
   * and the stream can create another reader.
   * https://streams.spec.whatwg.org/#default-reader-release-lock
   * @return {void}
   */
  releaseLock() {}
}

/**
 * A controller for a lite ReadableStream.
 *
 * Provides the enqueue(), error(), and close() methods.
 * @template T
 * @interface
 */
class ReadableStreamDefaultController {
  /**
   * Signals that the ReadableStream should close. The ReadableStream will
   * actually close once all of its chunks have been read.
   * https://streams.spec.whatwg.org/#rs-default-controller-close
   * @return {void}
   */
  close() {}

  /**
   * Enqueues a new chunk into the stream that can be read.
   * https://streams.spec.whatwg.org/#rs-default-controller-enqueue
   * @param {T} chunk
   */
  enqueue(chunk) {}

  /**
   * Closes the stream with an error. Any future interactions with the
   * controller will throw an error.
   * https://streams.spec.whatwg.org/#rs-default-controller-error
   * @param {*} e
   */
  error(e) {}
}

/**
 * The underlying source for a lite ReadableStream.
 * @template T
 * @record
 */
class ReadableStreamUnderlyingSource {
  constructor() {
    /**
     * A start method that is called when the ReadableStream is constructed.
     *
     * For the purpose of the lite version, this method is not optional,
     * and the return value is not used. In other versions, a Promise return
     * value will prevent calls to pull until the Promise is resolved.
     * @type {(function(!ReadableStreamDefaultController<T>):
     *     (!Promise<undefined>|undefined))|undefined}
     * https://streams.spec.whatwg.org/#dom-underlying-source-start
     */
    this.start;
  }
}

exports = {
  ReadableStream,
  ReadableStreamDefaultController,
  ReadableStreamDefaultReader,
  ReadableStreamUnderlyingSource,
};

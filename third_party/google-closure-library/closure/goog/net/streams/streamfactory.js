/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview the factory for creating stream objects.
 */

goog.module('goog.net.streams.streamFactory');

const NodeReadableStream = goog.requireType('goog.net.streams.NodeReadableStream');
const XhrIo = goog.requireType('goog.net.XhrIo');
const asserts = goog.require('goog.asserts');
const {XhrNodeReadableStream} = goog.require('goog.net.streams.xhrNodeReadableStream');
const {XhrStreamReader} = goog.require('goog.net.streams.xhrStreamReader');


/**
 * Creates a new NodeReadableStream object using goog.net.xhrio as the
 * underlying HTTP request.
 *
 * The XhrIo object should not have been sent to the network via its send()
 * method. NodeReadableStream callbacks are expected to be registered before
 * XhrIo.send() is invoked. The behavior of the stream is undefined if
 * otherwise. After send() is called, the lifecycle events are expected to
 * be handled directly via the stream API.
 *
 * If a binary response (e.g. protobuf) is expected, the caller should configure
 * the xhrIo by setResponseType(goog.net.XhrIo.ResponseType.ARRAY_BUFFER)
 * before xhrIo.send() is invoked.
 *
 * States specific to the xhr may be accessed before or after send() is called
 * as long as those operations are safe, e.g. configuring headers and options.
 *
 * Timeout (deadlines), cancellation (abort) should be applied to
 * XhrIo directly and the stream object will respect any life cycle events
 * trigger by those actions.
 *
 * Note for the release pkg:
 *   "--define goog.net.XmlHttpDefines.ASSUME_NATIVE_XHR=true"
 *   disable asserts
 *
 * @param {!XhrIo} xhr The XhrIo object with its response body to
 * be handled by NodeReadableStream.
 * @return {?NodeReadableStream} the newly created stream or
 * null if streaming response is not supported by the current User Agent.
 */
function createXhrNodeReadableStream(xhr) {
  'use strict';
  asserts.assert(!xhr.isActive(), 'XHR is already sent.');

  if (!XhrStreamReader.isStreamingSupported()) {
    return null;
  }

  const reader = new XhrStreamReader(xhr);
  return new XhrNodeReadableStream(reader);
}

exports = {createXhrNodeReadableStream};

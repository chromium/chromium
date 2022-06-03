/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The utils for creating new stream parsers.
 */

goog.module('goog.net.streams.streamParsers');

const Base64PbStreamParser = goog.require('goog.net.streams.Base64PbStreamParser');
const JsonStreamParser = goog.require('goog.net.streams.JsonStreamParser');
const PbJsonStreamParser = goog.require('goog.net.streams.PbJsonStreamParser');
const PbStreamParser = goog.require('goog.net.streams.PbStreamParser');
const StreamParser = goog.requireType('goog.net.streams.StreamParser');
const XhrIo = goog.require('goog.net.XhrIo');
const log = goog.require('goog.log');


/**
 * Returns a parser that supports the given content-type (mime) and
 * content-transfer-encoding.
 * @param {!XhrIo} io
 * @return {?StreamParser} a parser or null if the content
 *    type or transfer encoding is unsupported.
 */
function getStreamParser(io) {
  'use strict';
  const logger = log.getLogger('goog.net.streams.streamParsers');

  let contentType = io.getStreamingResponseHeader(XhrIo.CONTENT_TYPE_HEADER);
  if (!contentType) {
    log.warning(logger, 'Content-Type unavailable: ' + contentType);
    return null;
  }
  contentType = contentType.toLowerCase();

  if (contentType.startsWith('application/json')) {
    if (contentType.startsWith('application/json+protobuf')) {
      return new PbJsonStreamParser();
    }
    return new JsonStreamParser();
  }

  if (contentType.startsWith('application/x-protobuf')) {
    const encoding =
        io.getStreamingResponseHeader(XhrIo.CONTENT_TRANSFER_ENCODING);
    if (!encoding) {
      return new PbStreamParser();
    }
    if (encoding.toLowerCase() == 'base64') {
      return new Base64PbStreamParser();
    }
    log.warning(
        logger,
        'Unsupported Content-Transfer-Encoding: ' + encoding +
            '\nFor Content-Type: ' + contentType);
    return null;
  }

  log.warning(logger, 'Unsupported Content-Type: ' + contentType);
  return null;
}

exports = {getStreamParser};

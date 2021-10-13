/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The default base64-encoded Protobuf stream parser.
 *
 * A composed parser that first applies base64 stream decoding (see
 * {@link goog.net.streams.Base64StreamDecoder}) followed by Protobuf stream
 * parsing (see {@link goog.net.streams.PbStreamParser}).
 */

goog.module('goog.net.streams.Base64PbStreamParser');

const Base64StreamDecoder = goog.require('goog.net.streams.Base64StreamDecoder');
const PbStreamParser = goog.require('goog.net.streams.PbStreamParser');
const StreamParser = goog.require('goog.net.streams.StreamParser');
const asserts = goog.require('goog.asserts');


/**
 * The default base64-encoded Protobuf stream parser.
 *
 * @constructor
 * @struct
 * @implements {StreamParser}
 * @final
 */
const Base64PbStreamParser = function() {
  /**
   * The current error message, if any.
   * @private {?string}
   */
  this.errorMessage_ = null;

  /**
   * The current position in the streamed data.
   * @private {number}
   */
  this.streamPos_ = 0;

  /**
   * Base64 stream decoder
   * @private @const {!Base64StreamDecoder}
   */
  this.base64Decoder_ = new Base64StreamDecoder();

  /**
   * Protobuf raw bytes stream parser
   * @private @const
   */
  this.pbParser_ = new PbStreamParser();
};


/** @override */
Base64PbStreamParser.prototype.isInputValid = function() {
  return this.errorMessage_ === null;
};


/** @override */
Base64PbStreamParser.prototype.getErrorMessage = function() {
  return this.errorMessage_;
};


/**
 * @param {string} input The current input string to be processed
 * @param {string} errorMsg Additional error message
 * @throws {!Error} Throws an error indicating where the stream is broken
 * @private
 */
Base64PbStreamParser.prototype.error_ = function(input, errorMsg) {
  this.errorMessage_ = 'The stream is broken @' + this.streamPos_ +
      '. Error: ' + errorMsg + '. With input:\n' + input;
  throw new Error(this.errorMessage_);
};

/**
 * @override
 * @return {boolean}
 */
Base64PbStreamParser.prototype.acceptsBinaryInput = function() {
  return false;
};

/** @override */
Base64PbStreamParser.prototype.parse = function(input) {
  asserts.assertString(input);

  if (this.errorMessage_ !== null) {
    this.error_(input, 'stream already broken');
  }

  let result = null;
  try {
    const rawBytes = this.base64Decoder_.decode(input);
    result = (rawBytes === null) ? null : this.pbParser_.parse(rawBytes);
  } catch (e) {
    this.error_(input, e.message);
  }

  this.streamPos_ += input.length;
  return result;
};


exports = Base64PbStreamParser;

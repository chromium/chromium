/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A stream parser of StreamBody message in Protobuf-JSON format.
 *
 * 1. StreamBody proto message is defined as following:
 *
 *    message StreamBody {
 *      repeated bytes messages = 1;
 *      google.rpc.Status status = 2;
 *    }
 *
 * 2. In Protobuf-JSON format, StreamBody is represented as a JSON array:
 *
 *    - [ [ message1, message2, ..., messageN ] ]  (no status)
 *    - [ null, status ]  (no message)
 *    - [ [ message1, message2, ..., messageN ] , status ]
 *
 * 3. All parsed messages and status will be delivered in a batch (array),
 *    with each constructed as {tag-id: content-string}.
 */

goog.module('goog.net.streams.PbJsonStreamParser');

const JsonStreamParser = goog.require('goog.net.streams.JsonStreamParser');
const StreamParser = goog.require('goog.net.streams.StreamParser');
const asserts = goog.require('goog.asserts');
const utils = goog.require('goog.net.streams.utils');


/**
 * A stream parser of StreamBody message in Protobuf-JSON format.
 *
 * @constructor
 * @struct
 * @implements {StreamParser}
 * @final
 */
const PbJsonStreamParser = function() {
  /**
   * Protobuf raw bytes stream parser
   * @private {?JsonStreamParser}
   */
  this.jsonStreamParser_ = null;

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
   * The current parser state.
   * @private {!State}
   */
  this.state_ = State.INIT;

  /**
   * The currently buffered result (parsed JSON objects).
   * @private {!Array<!Object>}
   */
  this.result_ = [];

  /**
   * Whether the status has been parsed.
   * @private {boolean}
   */
  this.statusParsed_ = false;
};


/**
 * The parser state.
 * @enum {number}
 */
const State = {
  INIT: 0,           // expecting the beginning "["
  ARRAY_OPEN: 1,     // expecting the message array or the msg-status separator
  MESSAGES: 2,       // expecting the message array
  MESSAGES_DONE: 3,  // expecting the msg-status separator or the ending "]"
  STATUS: 4,         // expecting the status
  ARRAY_END: 5,      // expecting NO more non-whitespace input
  INVALID: 6,        // the stream has become invalid
};


/** @override */
PbJsonStreamParser.prototype.isInputValid = function() {
  return this.errorMessage_ === null;
};


/** @override */
PbJsonStreamParser.prototype.getErrorMessage = function() {
  return this.errorMessage_;
};

/**
 * @override
 * @return {boolean}
 */
PbJsonStreamParser.prototype.acceptsBinaryInput = function() {
  return false;
};

/** @override */
PbJsonStreamParser.prototype.parse = function(input) {
  asserts.assertString(input);

  const parser = this;
  let pos = 0;
  while (pos < input.length) {
    if ((parser.state_ !== State.MESSAGES) && !readMore()) {
      return null;
    }

    switch (parser.state_) {
      case State.INVALID: {
        reportError('stream already broken');
        break;
      }
      case State.INIT: {
        if (input[pos] === '[') {
          parser.state_ = State.ARRAY_OPEN;
          pos++;
          parser.streamPos_++;
        } else {
          reportError('unexpected input token');
        }
        break;
      }
      case State.ARRAY_OPEN: {
        if (input[pos] === '[') {
          parser.state_ = State.MESSAGES;
          resetJsonStreamParser();
          // Feed the '[' again in the next loop.
        } else if (input[pos] === ',' || input.substr(pos, 5) == 'null,') {
          parser.state_ = State.MESSAGES_DONE;
          // Feed the ',' again in the next loop.
        } else if (input[pos] === ']') {
          parser.state_ = State.ARRAY_END;
          pos++;
          parser.streamPos_++;
        } else {
          reportError('unexpected input token');
        }
        break;
      }
      case State.MESSAGES: {
        const messages = parser.jsonStreamParser_.parse(input.substring(pos));
        addResultMessages(messages);

        if (!parser.jsonStreamParser_.done()) {
          parser.streamPos_ += input.length - pos;
          pos = input.length;  // end the loop
        } else {
          parser.state_ = State.MESSAGES_DONE;
          const extra = parser.jsonStreamParser_.getExtraInput();
          parser.streamPos_ += input.length - pos - extra.length;
          input = extra;
          pos = 0;
        }
        break;
      }
      case State.MESSAGES_DONE: {
        if (input[pos] === ',' || input.substr(pos, 5) == 'null,') {
          parser.state_ = State.STATUS;
          resetJsonStreamParser();
          // Feed a dummy "[" to match the ending "]".
          parser.jsonStreamParser_.parse('[');
          pos += (input[pos] === ',' ? 1 : 5);
          parser.streamPos_++;
        } else if (input[pos] === ']') {
          parser.state_ = State.ARRAY_END;
          pos++;
          parser.streamPos_++;
        }
        break;
      }
      case State.STATUS: {
        const status = parser.jsonStreamParser_.parse(input.substring(pos));
        addResultStatus(status);

        if (!parser.jsonStreamParser_.done()) {
          parser.streamPos_ += input.length - pos;
          pos = input.length;  // end the loop
        } else {
          parser.state_ = State.ARRAY_END;
          const extra = parser.jsonStreamParser_.getExtraInput();
          parser.streamPos_ += input.length - pos - extra.length;
          input = extra;
          pos = 0;
        }
        break;
      }
      case State.ARRAY_END: {
        reportError('extra input after stream end');
        break;
      }
    }
  }

  if (parser.result_.length > 0) {
    const results = parser.result_;
    parser.result_ = [];
    return results;
  }
  return null;


  /**
   * @param {string} errorMessage Additional error message
   * @throws {!Error} Throws an error indicating where the stream is broken
   */
  function reportError(errorMessage) {
    parser.state_ = State.INVALID;
    parser.errorMessage_ = 'The stream is broken @' + parser.streamPos_ + '/' +
        pos + '. Error: ' + errorMessage + '. With input:\n';
    throw new Error(parser.errorMessage_);
  }


  /**
   * Advances to the first non-whitespace input character.
   *
   * @return {boolean} return false if no more non-whitespace input character
   */
  function readMore() {
    while (pos < input.length) {
      if (!utils.isJsonWhitespace(input[pos])) {
        return true;
      }
      pos++;
      parser.streamPos_++;
    }
    return false;
  }

  function resetJsonStreamParser() {
    parser.jsonStreamParser_ = new JsonStreamParser(
        {allowCompactJsonArrayFormat: true, deliverMessageAsRawString: true});
  }

  /** @param {?Array<string>} messages Parsed messages */
  function addResultMessages(messages) {
    if (messages) {
      for (let i = 0; i < messages.length; i++) {
        const tagged = {};
        tagged[1] = messages[i];
        parser.result_.push(tagged);
      }
    }
  }

  /** @param {?Array<string>} status Parsed status */
  function addResultStatus(status) {
    if (status) {
      if (parser.statusParsed_ || status.length > 1) {
        reportError('extra status: ' + status);
      }
      parser.statusParsed_ = true;

      const tagged = {};
      tagged[2] = status[0];
      parser.result_.push(tagged);
    }
  }
};


exports = PbJsonStreamParser;

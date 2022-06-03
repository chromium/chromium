/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Codec functions of the v8 wire protocol. Eventually we'd want
 * to support pluggable wire-format to improve wire efficiency and to enable
 * binary encoding. Such support will require an interface class, which
 * will be added later.
 *
 */


goog.provide('goog.labs.net.webChannel.WireV8');

goog.require('goog.asserts');
goog.require('goog.collections.maps');
goog.require('goog.json');
goog.require('goog.json.NativeJsonProcessor');
goog.require('goog.labs.net.webChannel.Wire');
goog.require('goog.structs');
goog.requireType('goog.string.Parser');



/**
 * The v8 codec class.
 *
 * @constructor
 * @struct
 */
goog.labs.net.webChannel.WireV8 = function() {
  'use strict';
  /**
   * Parser for a response payload. The parser should return an array.
   * @private {!goog.string.Parser}
   */
  this.parser_ = new goog.json.NativeJsonProcessor();
};


goog.scope(function() {
'use strict';
const WireV8 = goog.labs.net.webChannel.WireV8;
const Wire = goog.labs.net.webChannel.Wire;


/**
 * Encodes a standalone message into the wire format.
 *
 * May throw exception if the message object contains any invalid elements.
 *
 * @param {!Object|!goog.collections.maps.MapLike} message The message data.
 *     V8 only support JS objects (or Map).
 * @param {!Array<string>} buffer The text buffer to write the message to.
 * @param {string=} opt_prefix The prefix for each field of the object.
 */
WireV8.prototype.encodeMessage = function(message, buffer, opt_prefix) {
  'use strict';
  const prefix = opt_prefix || '';
  try {
    goog.structs.forEach(message, function(value, key) {
      'use strict';
      let encodedValue = value;
      if (goog.isObject(value)) {
        encodedValue = goog.json.serialize(value);
      }  // keep the fast-path for primitive types
      buffer.push(prefix + key + '=' + encodeURIComponent(encodedValue));
    });
  } catch (ex) {
    // We send a map here because lots of the retry logic relies on map IDs,
    // so we have to send something (possibly redundant).
    buffer.push(
        prefix + 'type' +
        '=' + encodeURIComponent('_badmap'));
    throw ex;
  }
};


/**
 * Encodes all the buffered messages of the forward channel.
 *
 * @param {!Array<Wire.QueuedMap>} messageQueue The message data.
 *     V8 only support JS objects.
 * @param {number} count The number of messages to be encoded.
 * @param {?function(!Object)} badMapHandler Callback for bad messages.
 * @return {string} the encoded messages
 */
WireV8.prototype.encodeMessageQueue = function(
    messageQueue, count, badMapHandler) {
  'use strict';
  let offset = -1;
  while (true) {
    const sb = ['count=' + count];
    // To save a bit of bandwidth, specify the base mapId and the rest as
    // offsets from it.
    if (offset == -1) {
      if (count > 0) {
        offset = messageQueue[0].mapId;
        sb.push('ofs=' + offset);
      } else {
        offset = 0;
      }
    } else {
      sb.push('ofs=' + offset);
    }
    let done = true;
    for (let i = 0; i < count; i++) {
      let mapId = messageQueue[i].mapId;
      const map = messageQueue[i].map;
      mapId -= offset;
      if (mapId < 0) {
        // redo the encoding in case of retry/reordering, plus extra space
        offset = Math.max(0, messageQueue[i].mapId - 100);
        done = false;
        continue;
      }
      try {
        this.encodeMessage(map, sb, 'req' + mapId + '_');
      } catch (ex) {
        if (badMapHandler) {
          badMapHandler(map);
        }
      }
    }
    if (done) {
      return sb.join('&');
    }
  }
};


/**
 * Decodes a standalone message received from the wire. May throw exception
 * if text is ill-formatted.
 *
 * Must be valid JSON as it is insecure to use eval() to decode JS literals;
 * and eval() is disallowed in Chrome apps too.
 *
 * Invalid JS literals include null array elements, quotas etc.
 *
 * @param {string} messageText The string content as received from the wire.
 * @return {*} The decoded message object.
 */
WireV8.prototype.decodeMessage = function(messageText) {
  'use strict';
  const response = this.parser_.parse(messageText);
  goog.asserts.assert(Array.isArray(response));  // throw exception
  return response;
};
});  // goog.scope

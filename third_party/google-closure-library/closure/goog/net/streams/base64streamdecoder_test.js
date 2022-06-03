/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.Base64StreamDecoderTest');
goog.setTestOnly();

const Base64StreamDecoder = goog.require('goog.net.streams.Base64StreamDecoder');
const asserts = goog.require('goog.testing.asserts');
const testSuite = goog.require('goog.testing.testSuite');

// Static test data
// clang-format off
const tests = [
  '', '',
  'f', 'Zg==',
  'fo', 'Zm8=',
  'foo', 'Zm9v',
  'foob', 'Zm9vYg==',
  'fooba', 'Zm9vYmE=',
  'foobar', 'Zm9vYmFy',
  'foobar', '  Zm  9v \t Ym \n Fy  ',  // whitespaces will be ignored

  '\xfb\xff\xbf\x4d', '+/+/TQ==',
  '\xfb\xff\xbf\x4d', '-_-_TQ..',  // websafe

  // non-ascii characters
  '\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94\xe5' +
      '\x85\xad\xe4\xb8\x83\xe5\x85\xab\xe4\xb9\x9d\xe5\x8d\x81',
  '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
];
// clang-format on

/**
 * @param {string} s The string
 * @return {!Array<number>} The UTF-16 codes of the characters of the string.
 */
function stringCodes(s) {
  let codes = [];
  for (let i = 0; i < s.length; i++) {
    codes.push(s.charCodeAt(i));
  }
  return codes;
}

testSuite({
  testSingleMessage() {
    const decoder = new Base64StreamDecoder();

    for (let i = 0; i < tests.length; i += 2) {
      const decoded = decoder.decode(tests[i + 1]);
      if (tests[i]) {
        assertElementsEquals(stringCodes(tests[i]), decoded);
      } else {
        assertNull(decoded);
      }
    }
  },

  testBadMessage() {
    const decoder = new Base64StreamDecoder();

    assertThrows(() => {
      decoder.decode('badchar!');
    });
    assertFalse(decoder.isInputValid());

    // decoder already invalidated
    assertThrows(() => {
      decoder.decode('abc');
    });
    assertFalse(decoder.isInputValid());
  },

  testMessagesInChunks() {
    const decoder = new Base64StreamDecoder();

    assertNull(decoder.decode('Zm'));
    assertNull(decoder.decode('9'));
    assertElementsEquals(stringCodes('foobar'), decoder.decode('vYmFyZm'));
    assertElementsEquals(stringCodes('foo'), decoder.decode('9v'));
    assertElementsEquals(stringCodes('barfoo'), decoder.decode('YmFyZm9v'));
    assertTrue(decoder.isInputValid());
  },
});

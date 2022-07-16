/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.PbStreamParserTest');
goog.setTestOnly();

const PbStreamParser = goog.require('goog.net.streams.PbStreamParser');
const asserts = goog.require('goog.testing.asserts');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

// clang-format off
const testMessage1 = {
  data: [
    0x0a, 0x00,                                            // msg: ''
    0x0a, 0x07, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  // msg: 'abcdefg'
    0x7a, 0x04, 0x70, 0x61, 0x64, 0x31,                    // padding: 'pad1'
    0x0a, 0x08,  // msg: (special chars)
    0x00, 0x01, 0x02, 0x03, 0x0a, 0xff, 0xfe, 0xfd, 0x7a, 0x00,  // padding: ''
    0x12, 0x17,  // status: (23 bytes long sub-message)
    0x08, 0xc8, 0x01, 0x12, 0x12, 0x73, 0x6f, 0x6d, 0x65, 0x74,
    0x68, 0x69, 0x6e, 0x67, 0x2d, 0x69, 0x73, 0x2d, 0x77, 0x72,
    0x6f, 0x6e, 0x67,
    0x7a, 0x02, 0x00, 0x00  // padding: {0x00, 0x00}
  ],
  parsed: [
    {1: []},
    {1: [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67]},
    {1: [0x00, 0x01, 0x02, 0x03, 0x0a, 0xff, 0xfe, 0xfd]},
    {2: [0x08, 0xc8, 0x01, 0x12, 0x12, 0x73, 0x6f, 0x6d, 0x65, 0x74, 0x68, 0x69,
         0x6e, 0x67, 0x2d, 0x69, 0x73, 0x2d, 0x77, 0x72, 0x6f, 0x6e, 0x67]},
  ],
};
// clang-format on

/**
 * @param {!Array<number>} bytes The input bytes
 * @return {!ArrayBuffer|!Array<number>} The input bytes in an ArrayBuffer.
 *     Falls back to native array if ArrayBuffer is not supported.
 */
function getInput(bytes) {
  if (typeof Uint8Array !== 'undefined') {
    return (new Uint8Array(bytes)).buffer;
  }
  return bytes;
}

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testSingleMessage() {
    const parser = new PbStreamParser();
    const input = getInput([0x0a, 0x05, 0xFF, 0xFE, 0x00, 0x01, 0x77]);
    let result = parser.parse(input);
    assertEquals(1, result.length);
    assertElementsEquals(['1'], googObject.getKeys(result[0]));
    assertElementsEquals([0xFF, 0xFE, 0x00, 0x01, 0x77], result[0][1]);

    result = parser.parse(getInput([]));
    assertNull(result);
  },

  testMultipleMessagesWithPadding() {
    const parser = new PbStreamParser();
    const result = parser.parse(getInput(testMessage1.data));
    const expected = testMessage1.parsed;

    assertEquals(expected.length, result.length);
    for (let i = 0; i < expected.length; i++) {
      /** @suppress {checkTypes} suppression added to enable type checking */
      const keys = googObject.getKeys(result[i]);
      assertElementsEquals(googObject.getKeys(expected[i]), keys);

      assertEquals(1, keys.length);
      assertElementsEquals(expected[i][keys[0]], result[i][keys[0]]);

      if (typeof Uint8Array !== 'undefined') {
        assertTrue(result[i][keys[0]] instanceof Uint8Array);
      } else {
        assertTrue(result[i][keys[0]] instanceof Array);
      }
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMessagesInChunks() {
    // clang-format off
    const data = [
      0x0a, 0x03, 0x61, 0x62, 0x63,
      0x0a, 0x03, 0x64, 0x65, 0x66,
      0x12, 0x03, 0x67, 0x68, 0x69,
    ];
    // clang-format on

    const parser = new PbStreamParser();

    let result = parser.parse(getInput(data.slice(0, 3)));
    assertNull(result);

    result = parser.parse(getInput(data.slice(3, 8)));
    assertEquals(1, result.length);
    assertElementsEquals(['1'], googObject.getKeys(result[0]));
    assertElementsEquals([0x61, 0x62, 0x63], result[0][1]);

    result = parser.parse(getInput(data.slice(8, 10)));
    assertEquals(1, result.length);
    assertElementsEquals(['1'], googObject.getKeys(result[0]));
    assertElementsEquals([0x64, 0x65, 0x66], result[0][1]);

    result = parser.parse(getInput(data.slice(10)));
    assertEquals(1, result.length);
    assertElementsEquals(['2'], googObject.getKeys(result[0]));
    assertElementsEquals([0x67, 0x68, 0x69], result[0][2]);
  },

  testInvalidInputs() {
    let parser;

    // wrong wire type
    parser = new PbStreamParser();
    assertThrows(() => {
      parser.parse(getInput([0x0b]));
    });
    // parser already invalidated
    assertThrows(() => {
      parser.parse(getInput([0x0a]));
    });

    // unknown tag
    parser = new PbStreamParser();
    assertThrows(() => {
      parser.parse([0x1a]);
    });

    // length too long
    parser = new PbStreamParser();
    assertThrows(() => {
      parser.parse(getInput([0x0a, 0xff, 0xff, 0xff, 0xff, 0x10]));
    });

    // length is going to be too long since more varint bytes are comming
    parser = new PbStreamParser();
    assertThrows(() => {
      parser.parse(getInput([0x0a, 0xff, 0xff, 0xff, 0xff, 0x80]));
    });
  },
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.Base64PbStreamParserTest');
goog.setTestOnly('goog.net.streams.Base64PbStreamParserTest');

const Base64PbStreamParser = goog.require('goog.net.streams.Base64PbStreamParser');
const base64 = goog.require('goog.crypt.base64');
const object = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

// Static test data
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
 * Encodes input with base64 encoding.
 * @param {!Array<number>} input The input bytes
 * @return {string} The encoded string
 */
function encodeBytes(input) {
  return base64.encodeByteArray(input, base64.Alphabet.WEBSAFE_DOT_PADDING);
}


testSuite({
  testSingleMessage: /**
                        @suppress {checkTypes} suppression added to enable type
                        checking
                      */
      function() {
        const parser = new Base64PbStreamParser();

        const input = 'CgX__gABdw==';
        let result = parser.parse(input);
        assertEquals(1, result.length);
        assertElementsEquals(['1'], object.getKeys(result[0]));
        assertElementsEquals([0xFF, 0xFE, 0x00, 0x01, 0x77], result[0][1]);

        if (typeof Uint8Array !== 'undefined') {
          assertTrue(result[0][1] instanceof Uint8Array);
        } else {
          assertTrue(result[0][1] instanceof Array);
        }

        result = parser.parse('');
        assertNull(result);
        assertTrue(parser.isInputValid());
      },

  testMultipleMessages: function() {
    const parser = new Base64PbStreamParser();
    const input = encodeBytes(testMessage1.data);
    const expected = testMessage1.parsed;
    const result = parser.parse(input);

    assertEquals(expected.length, result.length);
    for (let i = 0; i < expected.length; i++) {
      /** @suppress {checkTypes} suppression added to enable type checking */
      const keys = object.getKeys(result[i]);
      assertElementsEquals(object.getKeys(expected[i]), keys);

      assertEquals(1, keys.length);
      assertElementsEquals(expected[i][keys[0]], result[i][keys[0]]);
    }
  },

  testInvalidInputs: function() {
    const parser1 = new Base64PbStreamParser();
    // invalid base-64 character
    assertThrows(function() { parser1.parse('badchar!'); });
    assertFalse(parser1.isInputValid());
    // parser already invalidated
    assertThrows(function() { parser1.parse('CgX__gABdw=='); });
    assertFalse(parser1.isInputValid());

    const parser2 = new Base64PbStreamParser();
    // invalid message tag
    assertThrows(function() { parser2.parse('GgGq'); });
    assertFalse(parser2.isInputValid());

    const parser3 = new Base64PbStreamParser();
    // message length too long
    assertThrows(function() { parser3.parse('Cv____8Q'); });
    assertFalse(parser3.isInputValid());
  },

  testMessagesInChunks: /**
                           @suppress {checkTypes} suppression added to enable
                           type checking
                         */
      function() {
        // clang-format off
    const data = [
      0x0a, 0x03, 0x61, 0x62, 0x63,
      0x0a, 0x03, 0x64, 0x65, 0x66,
      0x12, 0x03, 0x67, 0x68, 0x69,
    ];
        // clang-format on

        const parser = new Base64PbStreamParser();

        let result = parser.parse(encodeBytes(data.slice(0, 3)));
        assertNull(result);

        result = parser.parse(encodeBytes(data.slice(3, 12)));
        assertEquals(2, result.length);
        assertElementsEquals(['1'], object.getKeys(result[0]));
        assertElementsEquals([0x61, 0x62, 0x63], result[0][1]);
        assertElementsEquals(['1'], object.getKeys(result[1]));
        assertElementsEquals([0x64, 0x65, 0x66], result[1][1]);

        result = parser.parse(encodeBytes(data.slice(12)));
        assertEquals(1, result.length);
        assertElementsEquals(['2'], object.getKeys(result[0]));
        assertElementsEquals([0x67, 0x68, 0x69], result[0][2]);
      },
});

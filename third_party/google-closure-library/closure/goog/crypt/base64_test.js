/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.base64Test');
goog.setTestOnly();

const base64 = goog.require('goog.crypt.base64');
const crypt = goog.require('goog.crypt');
const testSuite = goog.require('goog.testing.testSuite');

const SUPPORT_TYPED_ARRAY = typeof Uint8Array === 'function';

// Static test data
// clang-format off
const tests = [
  ['', ['', '', '', '', '']],
  ['f', ['Zg==', 'Zg', 'Zg==', 'Zg..', 'Zg']],
  ['fo', ['Zm8=', 'Zm8', 'Zm8=', 'Zm8.', 'Zm8']],
  ['foo', ['Zm9v', 'Zm9v', 'Zm9v', 'Zm9v', 'Zm9v']],
  ['foob', ['Zm9vYg==', 'Zm9vYg', 'Zm9vYg==', 'Zm9vYg..', 'Zm9vYg']],
  ['fooba', ['Zm9vYmE=', 'Zm9vYmE', 'Zm9vYmE=', 'Zm9vYmE.', 'Zm9vYmE']],
  ['foobar', ['Zm9vYmFy', 'Zm9vYmFy', 'Zm9vYmFy', 'Zm9vYmFy', 'Zm9vYmFy']],

  // Testing non-ascii characters (1-10 in chinese)
  [
    '\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94\xe5' +
        '\x85\xad\xe4\xb8\x83\xe5\x85\xab\xe4\xb9\x9d\xe5\x8d\x81',
    [
      '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
      '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
      '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
      '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
      '5LiA5LqM5LiJ5Zub5LqU5YWt5LiD5YWr5Lmd5Y2B',
    ],
  ],

  // Testing for web-safe alphabets
  [
    '>>>???>>>???=/',
    ['Pj4+Pz8/Pj4+Pz8/PS8=', 'Pj4+Pz8/Pj4+Pz8/PS8', 'Pj4-Pz8_Pj4-Pz8_PS8=', 'Pj4-Pz8_Pj4-Pz8_PS8.', 'Pj4-Pz8_Pj4-Pz8_PS8'],
  ],
];
// clang-format on

/**
 * Asserts encodings
 * @param {string} input an input string.
 * @param {!Array<string>} expectedOutputs expected outputs in the order of
 *     base64.Alphabet enum.
 */
function assertEncodings(input, expectedOutputs) {
  const arr = crypt.stringToByteArray(input);

  // encodeString
  for (const name in base64.Alphabet) {
    const alphabet = base64.Alphabet[name];
    assertEquals(
        base64.encodeString(input, alphabet), expectedOutputs[alphabet]);
  }
  assertEquals(
      base64.encodeString(input),  // default case
      expectedOutputs[base64.Alphabet.DEFAULT]);

  // encodeByteArray with Array<number>
  for (const name in base64.Alphabet) {
    const alphabet = base64.Alphabet[name];
    assertEquals(
        base64.encodeByteArray(arr, alphabet), expectedOutputs[alphabet]);
  }
  assertEquals(
      base64.encodeByteArray(arr),  // default case
      expectedOutputs[base64.Alphabet.DEFAULT]);

  // encodeByteArray with Uint8Array
  if (SUPPORT_TYPED_ARRAY) {
    const uint8Arr = new Uint8Array(arr);
    for (const name in base64.Alphabet) {
      const alphabet = base64.Alphabet[name];
      assertEquals(
          base64.encodeByteArray(uint8Arr, alphabet),
          expectedOutputs[alphabet]);
    }
    assertEquals(
        base64.encodeByteArray(uint8Arr),  // default case
        expectedOutputs[base64.Alphabet.DEFAULT]);
  }
}

/**
 * Assert decodings
 * @param {!Array<string>} inputs input strings in various encodings.
 * @param {string} stringOutput expected output in string.
 */
function assertDecodings(inputs, stringOutput) {
  const arrOutput = crypt.stringToByteArray(stringOutput);
  const uint8ArrOutput = SUPPORT_TYPED_ARRAY ? new Uint8Array(arrOutput) : null;

  for (let i = 0; i < inputs.length; i++) {
    const input = inputs[i];

    // decodeString
    assertEquals(base64.decodeString(input, true), stringOutput);

    if (i === 0) {
      // For Alphabet.DEFAULT, test with native decoder too
      assertEquals(base64.decodeString(input), stringOutput);
    }

    // decodeStringToByteArray
    assertArrayEquals(base64.decodeStringToByteArray(input), arrOutput);
    // Check that obsolete websafe param has no effect.
    assertArrayEquals(base64.decodeStringToByteArray(input, true), arrOutput);

    // decodeStringToUint8Array
    if (SUPPORT_TYPED_ARRAY) {
      assertObjectEquals(
          base64.decodeStringToUint8Array(input), uint8ArrOutput);
    }
  }
}

testSuite({
  testEncode() {
    for (const [str, encodedStrings] of tests) {
      assertEncodings(str, encodedStrings);
    }
  },

  testDecode() {
    for (const [str, encodedStrings] of tests) {
      assertDecodings(encodedStrings, str);
    }
  },

  testDecodeMalformedInput() {
    // Test parsing malformed characters
    assertThrows('Didn\'t throw on malformed input', () => {
      base64.decodeStringToByteArray('foooooo)oooo');
    });

    // Test parsing malformed characters
    assertThrows('Didn\'t throw on malformed input', () => {
      base64.decodeStringToUint8Array('foooooo)oooo');
    });
  },

  testDecodeIgnoresSpace() {
    const spaceTests = [
      // [encoded, expected decoded]
      [' \n\t\r', ''],
      ['Z g =\n=', 'f'],
      ['Zm 8=', 'fo'],
      [' Zm 9v', 'foo'],
      ['Zm9v Yg ==\t ', 'foob'],
      ['\nZ m9  vYm\n E=', 'fooba'],
      ['  \nZ \tm9v YmFy  ', 'foobar'],
    ];

    for (const [encoded, decoded] of spaceTests) {
      const decodedArr = crypt.stringToByteArray(decoded);

      assertEquals(base64.decodeString(encoded), decoded);        // native
      assertEquals(base64.decodeString(encoded, true), decoded);  // custom
      assertArrayEquals(base64.decodeStringToByteArray(encoded), decodedArr);

      if (SUPPORT_TYPED_ARRAY) {
        const decodedUint8Arr = new Uint8Array(decodedArr);
        assertObjectEquals(
            base64.decodeStringToUint8Array(encoded), decodedUint8Arr);
      }
    }
  },
});

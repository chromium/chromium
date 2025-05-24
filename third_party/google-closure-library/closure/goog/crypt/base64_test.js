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
    {
      binary: '\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94' +
          '\xe5\x85\xad\xe4\xb8\x83\xe5\x85\xab\xe4\xb9\x9d\xe5\x8d\x81',
      text: '一二三四五六七八九十',
    },
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
    [
      'Pj4+Pz8/Pj4+Pz8/PS8=',
      'Pj4+Pz8/Pj4+Pz8/PS8',
      'Pj4-Pz8_Pj4-Pz8_PS8=',
      'Pj4-Pz8_Pj4-Pz8_PS8.',
      'Pj4-Pz8_Pj4-Pz8_PS8',
    ],
  ],
];
// clang-format on

/**
 * Asserts encodings
 * @param {string|{binary: string, text: string}} input an input string.
 * @param {!Array<string>} expectedOutputs expected outputs in the order of
 *     base64.Alphabet enum.
 */
function assertEncodings(input, expectedOutputs) {
  const {text, binary} =
      typeof input === 'string' ? {text: input, binary: input} : input;
  const arr = crypt.stringToByteArray(binary);

  // quick validity test
  assertArrayEquals(arr, crypt.stringToUtf8ByteArray(text));

  // encodeString
  for (const name in base64.Alphabet) {
    const alphabet = base64.Alphabet[name];
    assertEquals(
        expectedOutputs[alphabet], base64.encodeStringUtf8(text, alphabet));
    assertEquals(expectedOutputs[alphabet], base64.encodeText(text, alphabet));
    assertEquals(
        expectedOutputs[alphabet], base64.encodeString(binary, alphabet));
    assertEquals(
        expectedOutputs[alphabet], base64.encodeBinaryString(binary, alphabet));
  }
  // default case
  assertEquals(
      expectedOutputs[base64.Alphabet.DEFAULT], base64.encodeText(text));
  assertEquals(
      expectedOutputs[base64.Alphabet.DEFAULT],
      base64.encodeBinaryString(binary));

  // encodeByteArray with Array<number>
  for (const name in base64.Alphabet) {
    const alphabet = base64.Alphabet[name];
    assertEquals(
        expectedOutputs[alphabet], base64.encodeByteArray(arr, alphabet));
  }
  // default case
  assertEquals(
      expectedOutputs[base64.Alphabet.DEFAULT], base64.encodeByteArray(arr));

  // encodeByteArray with Uint8Array
  if (SUPPORT_TYPED_ARRAY) {
    const uint8Arr = new Uint8Array(arr);
    for (const name in base64.Alphabet) {
      const alphabet = base64.Alphabet[name];
      assertEquals(
          expectedOutputs[alphabet],
          base64.encodeByteArray(uint8Arr, alphabet));
    }
    // default case
    assertEquals(
        expectedOutputs[base64.Alphabet.DEFAULT],
        base64.encodeByteArray(uint8Arr));
  }
}

/**
 * Assert decodings
 * @param {!Array<string>} inputs input strings in various encodings.
 * @param {string|{text: string, binary: string}} expectedOutput expected output
 *     in string (optionally split out for text/binary).
 */
function assertDecodings(inputs, expectedOutput) {
  const textOutput =
      typeof expectedOutput === 'string' ? expectedOutput : expectedOutput.text;
  const binaryOutput = typeof expectedOutput === 'string' ?
      expectedOutput :
      expectedOutput.binary;
  const arrOutput = crypt.stringToByteArray(binaryOutput);
  const uint8ArrOutput = SUPPORT_TYPED_ARRAY ? new Uint8Array(arrOutput) : null;

  // Quick validity check that decoding the text version is equivalent.
  assertArrayEquals(arrOutput, crypt.stringToUtf8ByteArray(textOutput));

  for (let i = 0; i < inputs.length; i++) {
    const input = inputs[i];

    // decodeString
    assertEquals(textOutput, base64.decodeStringUtf8(input, true));
    assertEquals(binaryOutput, base64.decodeString(input, true));
    assertEquals(textOutput, base64.decodeToText(input, true));
    assertEquals(binaryOutput, base64.decodeToBinaryString(input, true));

    if (i === 0) {
      // For Alphabet.DEFAULT, test with native decoder too
      assertEquals(textOutput, base64.decodeStringUtf8(input));
      assertEquals(binaryOutput, base64.decodeString(input));
      assertEquals(textOutput, base64.decodeToText(input));
      assertEquals(binaryOutput, base64.decodeToBinaryString(input));
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

      // native
      assertEquals(base64.decodeToBinaryString(encoded), decoded);
      assertEquals(base64.decodeToText(encoded), decoded);
      // custom
      assertEquals(base64.decodeToBinaryString(encoded, true), decoded);
      assertEquals(base64.decodeToText(encoded, true), decoded);

      assertArrayEquals(base64.decodeStringToByteArray(encoded), decodedArr);

      if (SUPPORT_TYPED_ARRAY) {
        const decodedUint8Arr = new Uint8Array(decodedArr);
        assertObjectEquals(
            base64.decodeStringToUint8Array(encoded), decodedUint8Arr);
      }
    }
  },
});

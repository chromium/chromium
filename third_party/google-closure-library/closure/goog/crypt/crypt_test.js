/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.cryptTest');
goog.setTestOnly();

const crypt = goog.require('goog.crypt');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

const UTF8_RANGES_BYTE_ARRAY =
    [0x00, 0x7F, 0xC2, 0x80, 0xDF, 0xBF, 0xE0, 0xA0, 0x80, 0xEF, 0xBF, 0xBF];

const UTF8_SURROGATE_PAIR_RANGES_BYTE_ARRAY = [
  0xF0, 0x90, 0x80, 0x80,  // \uD800\uDC00
  0xF0, 0x90, 0x8F, 0xBF,  // \uD800\uDFFF
  0xF4, 0x8F, 0xB0, 0x80,  // \uDBFF\uDC00
  0xF4, 0x8F, 0xBF, 0xBF   // \uDBFF\uDFFF
];

const UTF8_RANGES_STRING = '\u0000\u007F\u0080\u07FF\u0800\uFFFF';

const UTF8_SURROGATE_PAIR_RANGES_STRING =
    '\uD800\uDC00\uD800\uDFFF\uDBFF\uDC00\uDBFF\uDFFF';

// Tests a one-megabyte byte array conversion to string.
// This would break on many JS implementations unless byteArrayToString
// split the input up.
// See discussion and bug report: http://goo.gl/LrWmZ9

testSuite({
  testStringToUtf8ByteArray() {
    // Known encodings taken from Java's String.getBytes("UTF8")

    assertArrayEquals(
        'ASCII', [72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100],
        crypt.stringToUtf8ByteArray('Hello, world'));

    assertArrayEquals(
        'Latin', [83, 99, 104, 195, 182, 110],
        crypt.stringToUtf8ByteArray('Sch\u00f6n'));

    assertArrayEquals(
        'limits of the first 3 UTF-8 character ranges', UTF8_RANGES_BYTE_ARRAY,
        crypt.stringToUtf8ByteArray(UTF8_RANGES_STRING));

    assertArrayEquals(
        'Surrogate Pair', UTF8_SURROGATE_PAIR_RANGES_BYTE_ARRAY,
        crypt.stringToUtf8ByteArray(UTF8_SURROGATE_PAIR_RANGES_STRING));
  },

  testUtf8ByteArrayToString() {
    // Known encodings taken from Java's String.getBytes("UTF8")

    assertEquals('ASCII', 'Hello, world', crypt.utf8ByteArrayToString([
      72,
      101,
      108,
      108,
      111,
      44,
      32,
      119,
      111,
      114,
      108,
      100,
    ]));

    assertEquals(
        'Latin', 'Sch\u00f6n',
        crypt.utf8ByteArrayToString([83, 99, 104, 195, 182, 110]));

    assertEquals(
        'limits of the first 3 UTF-8 character ranges', UTF8_RANGES_STRING,
        crypt.utf8ByteArrayToString(UTF8_RANGES_BYTE_ARRAY));

    assertEquals(
        'Surrogate Pair', UTF8_SURROGATE_PAIR_RANGES_STRING,
        crypt.utf8ByteArrayToString(UTF8_SURROGATE_PAIR_RANGES_BYTE_ARRAY));
  },

  /**
   * Same as testUtf8ByteArrayToString but with Uint8Array instead of
   * Array<number>.
   */
  testUint8ArrayToString() {
    if (!globalThis.Uint8Array) {
      // Uint8Array not supported.
      return;
    }

    let arr = new Uint8Array(
        [72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100]);
    assertEquals('ASCII', 'Hello, world', crypt.utf8ByteArrayToString(arr));

    arr = new Uint8Array([83, 99, 104, 195, 182, 110]);
    assertEquals('Latin', 'Sch\u00f6n', crypt.utf8ByteArrayToString(arr));

    arr = new Uint8Array(UTF8_RANGES_BYTE_ARRAY);
    assertEquals(
        'limits of the first 3 UTF-8 character ranges', UTF8_RANGES_STRING,
        crypt.utf8ByteArrayToString(arr));
  },

  testByteArrayToString() {
    assertEquals('', crypt.byteArrayToString([]));
    assertEquals('abc', crypt.byteArrayToString([97, 98, 99]));
  },

  testHexToByteArray() {
    assertElementsEquals(
        [202, 254, 222, 173],
        // Java magic number
        crypt.hexToByteArray('cafedead'));

    assertElementsEquals(
        [222, 173, 190, 239],
        // IBM magic number
        crypt.hexToByteArray('DEADBEEF'));
  },

  testByteArrayToHex() {
    assertEquals(
        // Java magic number
        'cafedead', crypt.byteArrayToHex([202, 254, 222, 173]));

    assertEquals(
        // IBM magic number
        'deadbeef', crypt.byteArrayToHex([222, 173, 190, 239]));

    assertEquals('c0:ff:ee', crypt.byteArrayToHex([192, 255, 238], ':'));
  },

  /**
     Same as testByteArrayToHex but with Uint8Array instead of Array<number>.
   */
  testUint8ArrayToHex() {
    if (globalThis.Uint8Array === undefined) {
      // Uint8Array not supported.
      return;
    }

    assertEquals(
        // Java magic number
        'cafedead', crypt.byteArrayToHex(new Uint8Array([202, 254, 222, 173])));

    assertEquals(
        // IBM magic number
        'deadbeef', crypt.byteArrayToHex(new Uint8Array([222, 173, 190, 239])));

    assertEquals(
        'c0:ff:ee', crypt.byteArrayToHex(new Uint8Array([192, 255, 238]), ':'));
  },

  testXorByteArray() {
    assertElementsEquals(
        [20, 83, 96, 66],
        crypt.xorByteArray([202, 254, 222, 173], [222, 173, 190, 239]));
  },

  /** Same as testXorByteArray but with Uint8Array instead of Array<number>. */
  testXorUint8Array() {
    if (globalThis.Uint8Array === undefined) {
      // Uint8Array not supported.
      return;
    }

    assertElementsEquals(
        [20, 83, 96, 66],
        crypt.xorByteArray(
            new Uint8Array([202, 254, 222, 173]),
            new Uint8Array([222, 173, 190, 239])));
  },

  testByteArrayToStringCallStack() {
    // One megabyte is 2 to the 20th.
    const count = Math.pow(2, 20);
    const bytes = [];
    for (let i = 0; i < count; i++) {
      bytes.push('A'.charCodeAt(0));
    }
    const str = crypt.byteArrayToString(bytes);
    assertEquals(googString.repeat('A', count), str);
  },
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.HmacTest');
goog.setTestOnly();

const Hmac = goog.require('goog.crypt.Hmac');
const Sha1 = goog.require('goog.crypt.Sha1');
const hashTester = goog.require('goog.crypt.hashTester');
const testSuite = goog.require('goog.testing.testSuite');

function stringToBytes(s) {
  const bytes = new Array(s.length);

  for (let i = 0; i < s.length; ++i) {
    bytes[i] = s.charCodeAt(i) & 255;
  }
  return bytes;
}

function hexToBytes(str) {
  const arr = [];

  for (let i = 0; i < str.length; i += 2) {
    arr.push(parseInt(str.substring(i, i + 2), 16));
  }

  return arr;
}

function bytesToHex(b) {
  const hexchars = '0123456789abcdef';
  const hexrep = new Array(b.length * 2);

  for (let i = 0; i < b.length; ++i) {
    hexrep[i * 2] = hexchars.charAt((b[i] >> 4) & 15);
    hexrep[i * 2 + 1] = hexchars.charAt(b[i] & 15);
  }
  return hexrep.join('');
}

/** helper to get an hmac of the given message with the given key. */
function getHmac(key, message, blockSize = undefined) {
  const hasher = new Sha1();
  const hmacer = new Hmac(hasher, key, blockSize);
  return bytesToHex(hmacer.getHmac(message));
}

testSuite({
  /** @suppress {checkTypes} hmac doesn't access strings */
  testBasicOperations() {
    const hmac = new Hmac(new Sha1(), 'key', 64);
    hashTester.runBasicTests(hmac);
  },

  /** @suppress {checkTypes} hmac doesn't access strings */
  testBasicOperationsWithNoBlockSize() {
    const hmac = new Hmac(new Sha1(), 'key');
    hashTester.runBasicTests(hmac);
  },

  testHmac() {
    // HMAC test vectors from:
    // http://tools.ietf.org/html/2202

    assertEquals(
        'test 1 failed', 'b617318655057264e28bc0b6fb378c8ef146be00',
        getHmac(
            hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b'),
            stringToBytes('Hi There')));

    assertEquals(
        'test 2 failed', 'effcdf6ae5eb2fa2d27416d5f184df9c259a7c79',
        getHmac(
            stringToBytes('Jefe'),
            stringToBytes('what do ya want for nothing?')));

    assertEquals(
        'test 3 failed', '125d7342b9ac11cd91a39af48aa17b4f63f175d3',
        getHmac(
            hexToBytes('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'),
            hexToBytes(
                'dddddddddddddddddddddddddddddddddddddddd' +
                'dddddddddddddddddddddddddddddddddddddddd' +
                'dddddddddddddddddddd')));

    assertEquals(
        'test 4 failed', '4c9007f4026250c6bc8414f9bf50c86c2d7235da',
        getHmac(
            hexToBytes('0102030405060708090a0b0c0d0e0f10111213141516171819'),
            hexToBytes(
                'cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd' +
                'cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd' +
                'cdcdcdcdcdcdcdcdcdcd')));

    assertEquals(
        'test 5 failed', '4c1a03424b55e07fe7f27be1d58bb9324a9a5a04',
        getHmac(
            hexToBytes('0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c'),
            stringToBytes('Test With Truncation')));

    assertEquals(
        'test 6 failed', 'aa4ae5e15272d00e95705637ce8a3b55ed402112',
        getHmac(
            hexToBytes(
                'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'),
            stringToBytes(
                'Test Using Larger Than Block-Size Key - Hash Key First')));

    assertEquals(
        'test 7 failed', 'b617318655057264e28bc0b6fb378c8ef146be00',
        getHmac(
            hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b'),
            stringToBytes('Hi There'), 64));

    assertEquals(
        'test 8 failed', '941f806707826395dc510add6a45ce9933db976e',
        getHmac(
            hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b'),
            stringToBytes('Hi There'), 32));
  },

  /** Regression test for Bug 12863104 */
  testUpdateWithLongKey() {
    // Calling update() then digest() should give the same result as just
    // calling getHmac()
    const key = hexToBytes(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');
    const message = 'Secret Message';
    const hmac = new Hmac(new Sha1(), key);
    hmac.update(message);
    const result1 = bytesToHex(hmac.digest());
    hmac.reset();
    const result2 = bytesToHex(hmac.getHmac(message));
    assertEquals('Results must be the same', result1, result2);
  },
});

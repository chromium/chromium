/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for CBC mode for block ciphers.
 */

/** @suppress {extraProvide} */
goog.module('goog.crypt.CbcTest');
goog.setTestOnly();

const Aes = goog.require('goog.crypt.Aes');
const Cbc = goog.require('goog.crypt.Cbc');
const crypt = goog.require('goog.crypt');
const testSuite = goog.require('goog.testing.testSuite');

function stringToBytes(s) {
  const bytes = new Array(s.length);
  for (let i = 0; i < s.length; ++i) bytes[i] = s.charCodeAt(i) & 255;
  return bytes;
}

function runCbcAesTest(
    keyBytes, initialVectorBytes, plainTextBytes, cipherTextBytes) {
  const aes = new Aes(keyBytes);
  const cbc = new Cbc(aes);

  const encryptedBytes = cbc.encrypt(plainTextBytes, initialVectorBytes);
  assertEquals(
      'Encrypted bytes should match cipher text.',
      crypt.byteArrayToHex(cipherTextBytes),
      crypt.byteArrayToHex(encryptedBytes));

  const decryptedBytes = cbc.decrypt(cipherTextBytes, initialVectorBytes);
  assertEquals(
      'Decrypted bytes should match plain text.',
      crypt.byteArrayToHex(plainTextBytes),
      crypt.byteArrayToHex(decryptedBytes));
}

testSuite({
  testAesCbcCipherAlgorithm() {
    // Test values from http://www.ietf.org/rfc/rfc3602.txt

    // Case #1
    runCbcAesTest(
        crypt.hexToByteArray('06a9214036b8a15b512e03d534120006'),
        crypt.hexToByteArray('3dafba429d9eb430b422da802c9fac41'),
        stringToBytes('Single block msg'),
        crypt.hexToByteArray('e353779c1079aeb82708942dbe77181a'));

    // Case #2
    runCbcAesTest(
        crypt.hexToByteArray('c286696d887c9aa0611bbb3e2025a45a'),
        crypt.hexToByteArray('562e17996d093d28ddb3ba695a2e6f58'),
        crypt.hexToByteArray(
            '000102030405060708090a0b0c0d0e0f' +
            '101112131415161718191a1b1c1d1e1f'),
        crypt.hexToByteArray(
            'd296cd94c2cccf8a3a863028b5e1dc0a' +
            '7586602d253cfff91b8266bea6d61ab1'));

    // Case #3
    runCbcAesTest(
        crypt.hexToByteArray('6c3ea0477630ce21a2ce334aa746c2cd'),
        crypt.hexToByteArray('c782dc4c098c66cbd9cd27d825682c81'),
        stringToBytes('This is a 48-byte message (exactly 3 AES blocks)'),
        crypt.hexToByteArray(
            'd0a02b3836451753d493665d33f0e886' +
            '2dea54cdb293abc7506939276772f8d5' +
            '021c19216bad525c8579695d83ba2684'));

    // Case #4
    runCbcAesTest(
        crypt.hexToByteArray('56e47a38c5598974bc46903dba290349'),
        crypt.hexToByteArray('8ce82eefbea0da3c44699ed7db51b7d9'),
        crypt.hexToByteArray(
            'a0a1a2a3a4a5a6a7a8a9aaabacadaeaf' +
            'b0b1b2b3b4b5b6b7b8b9babbbcbdbebf' +
            'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf' +
            'd0d1d2d3d4d5d6d7d8d9dadbdcdddedf'),
        crypt.hexToByteArray(
            'c30e32ffedc0774e6aff6af0869f71aa' +
            '0f3af07a9a31a9c684db207eb0ef8e4e' +
            '35907aa632c3ffdf868bb7b29d3d46ad' +
            '83ce9f9a102ee99d49a53e87f4c3da55'));
  },
});

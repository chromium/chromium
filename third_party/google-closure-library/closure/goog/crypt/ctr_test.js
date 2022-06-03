/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for CTR mode for block ciphers.
 */


goog.module('goog.crypt.CtrTest');
goog.setTestOnly('goog.crypt.CtrTest');

const Aes = goog.require('goog.crypt.Aes');
const Ctr = goog.require('goog.crypt.Ctr');
const googCrypt = goog.require('goog.crypt');
const testSuite = goog.require('goog.testing.testSuite');


/**
 * Asserts the given parameters allow encryption and decryption from and to the
 * given plaintext/ciphertext.
 *
 * @param {!Array<number>|!Uint8Array} keyBytes
 * @param {!Array<number>|!Uint8Array} initialVectorBytes
 * @param {!Array<number>|!Uint8Array} plainTextBytes
 * @param {!Array<number>|!Uint8Array} cipherTextBytes
 */
function runCtrAesTest(
    keyBytes, initialVectorBytes, plainTextBytes, cipherTextBytes) {
  /** @suppress {checkTypes} suppression added to enable type checking */
  const aes = new Aes(keyBytes);
  const ctr = new Ctr(aes);

  const encryptedBytes = ctr.encrypt(plainTextBytes, initialVectorBytes);
  assertEquals(
      'Encrypted bytes should match cipher text.',
      googCrypt.byteArrayToHex(encryptedBytes),
      googCrypt.byteArrayToHex(cipherTextBytes));

  const decryptedBytes = ctr.decrypt(cipherTextBytes, initialVectorBytes);
  assertEquals(
      'Decrypted bytes should match plain text.',
      googCrypt.byteArrayToHex(decryptedBytes),
      googCrypt.byteArrayToHex(plainTextBytes));
}

/**
 * Asserts Ctr.incrementBigEndianCounter turns the first parameter
 * into the second.
 * @param {string} toIncrement
 * @param {string} expectedResult
 * @suppress {visibility} suppression added to enable type checking
 */
function assertIncEquals(toIncrement, expectedResult) {
  const counter = googCrypt.hexToByteArray(toIncrement);
  Ctr.incrementBigEndianCounter_(counter);
  assertEquals(expectedResult, googCrypt.byteArrayToHex(counter));
}

testSuite({
  testIncrementCounter() {
    assertIncEquals('', '');

    assertIncEquals('00', '01');
    assertIncEquals('09', '0a');
    assertIncEquals('0e', '0f');
    assertIncEquals('0f', '10');
    assertIncEquals('1f', '20');
    assertIncEquals('ff', '00');  // no length extension

    assertIncEquals('0000', '0001');
    assertIncEquals('00f0', '00f1');
    assertIncEquals('00ff', '0100');
    assertIncEquals('0f00', '0f01');
    assertIncEquals('0fff', '1000');
    assertIncEquals('1000', '1001');
    assertIncEquals('ff00', 'ff01');
    assertIncEquals('ff0f', 'ff10');
    assertIncEquals('ffff', '0000');

    assertIncEquals(
        'ffffffffffffffffffffffffffffffffffffffffffffffff',
        '000000000000000000000000000000000000000000000000');
  },

  testAes128CtrCipherAlgorithm() {
    // Test vectors from NIST sp800-38a, p 55
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf

    // Case #1, no chaining
    runCtrAesTest(
        googCrypt.hexToByteArray('2b7e151628aed2a6abf7158809cf4f3c'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray('6bc1bee22e409f96e93d7e117393172a'),
        googCrypt.hexToByteArray('874d6191b620e3261bef6864990db6ce'));

    // Case #2, three blocks
    runCtrAesTest(
        googCrypt.hexToByteArray('2b7e151628aed2a6abf7158809cf4f3c'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray(
            '6bc1bee22e409f96e93d7e117393172a' +
            'ae2d8a571e03ac9c9eb76fac45af8e51' +
            '30c81c46a35ce411e5fbc1191a0a52ef'),
        googCrypt.hexToByteArray(
            '874d6191b620e3261bef6864990db6ce' +
            '9806f66b7970fdff8617187bb9fffdff' +
            '5ae4df3edbd5d35e5b4f09020db03eab'));

    // Case #3, plaintext length not a multiple of blocksize
    runCtrAesTest(
        googCrypt.hexToByteArray('2b7e151628aed2a6abf7158809cf4f3c'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray(
            '6bc1bee22e409f96e93d7e117393172a' +
            'ae2d8a571e03ac9c9eb76fac45af8e51' +
            '30c81c'),
        googCrypt.hexToByteArray(
            '874d6191b620e3261bef6864990db6ce' +
            '9806f66b7970fdff8617187bb9fffdff' +
            '5ae4df'));
  },

  testAes192CtrCipherAlgorithm() {
    // Test vectors from NIST sp800-38a, p 56
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
    // Key block is weird, that's normal: 192 is one block and a half.


    // Case #1, no chaining
    runCtrAesTest(
        googCrypt.hexToByteArray(
            '8e73b0f7da0e6452c810f32b809079e5' +
            '62f8ead2522c6b7b'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray('6bc1bee22e409f96e93d7e117393172a'),
        googCrypt.hexToByteArray('1abc932417521ca24f2b0459fe7e6e0b'));

    // Case #2, three blocks
    runCtrAesTest(
        googCrypt.hexToByteArray(
            '8e73b0f7da0e6452c810f32b809079e5' +
            '62f8ead2522c6b7b'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray(
            '6bc1bee22e409f96e93d7e117393172a' +
            'ae2d8a571e03ac9c9eb76fac45af8e51' +
            '30c81c46a35ce411e5fbc1191a0a52ef'),
        googCrypt.hexToByteArray(
            '1abc932417521ca24f2b0459fe7e6e0b' +
            '090339ec0aa6faefd5ccc2c6f4ce8e94' +
            '1e36b26bd1ebc670d1bd1d665620abf7'));
  },

  testAes256CtrCipherAlgorithm() {
    // Test vectors from NIST sp800-38a, p 57
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf

    // Case #1, no chaining
    runCtrAesTest(
        googCrypt.hexToByteArray(
            '603deb1015ca71be2b73aef0857d7781' +
            '1f352c073b6108d72d9810a30914dff4'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray('6bc1bee22e409f96e93d7e117393172a'),
        googCrypt.hexToByteArray('601ec313775789a5b7a7f504bbf3d228'));


    // Case #2, three blocks
    runCtrAesTest(
        googCrypt.hexToByteArray(
            '603deb1015ca71be2b73aef0857d7781' +
            '1f352c073b6108d72d9810a30914dff4'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray(
            '6bc1bee22e409f96e93d7e117393172a' +
            'ae2d8a571e03ac9c9eb76fac45af8e51' +
            '30c81c46a35ce411e5fbc1191a0a52ef'),
        googCrypt.hexToByteArray(
            '601ec313775789a5b7a7f504bbf3d228' +
            'f443e3ca4d62b59aca84e990cacaf5c5' +
            '2b0930daa23de94ce87017ba2d84988d'));

    // Case #3, plaintext length not a multiple of blocksize
    runCtrAesTest(
        googCrypt.hexToByteArray(
            '603deb1015ca71be2b73aef0857d7781' +
            '1f352c073b6108d72d9810a30914dff4'),
        googCrypt.hexToByteArray('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff'),
        googCrypt.hexToByteArray(
            '6bc1bee22e409f96e93d7e117393172a' +
            'ae2d8a571e03ac9c9eb76fac45af8e51' +
            '30c81c46a35ce411e5fbc1191a0a52'),
        googCrypt.hexToByteArray(
            '601ec313775789a5b7a7f504bbf3d228' +
            'f443e3ca4d62b59aca84e990cacaf5c5' +
            '2b0930daa23de94ce87017ba2d8498'));
  },
});

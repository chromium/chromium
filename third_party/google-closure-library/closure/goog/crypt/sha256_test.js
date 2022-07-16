/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.Sha256Test');
goog.setTestOnly();

const Sha256 = goog.require('goog.crypt.Sha256');
const crypt = goog.require('goog.crypt');
const hashTester = goog.require('goog.crypt.hashTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const sha256 = new Sha256();
    hashTester.runBasicTests(sha256);
  },

  /** @suppress {visibility} accessing private properties */
  testHashing() {
    // Some test vectors from:
    // csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf

    const sha256 = new Sha256();

    // Empty message.
    sha256.update([]);
    assertEquals(
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
        crypt.byteArrayToHex(sha256.digest()));

    // NIST one block test vector.
    sha256.reset();
    sha256.update(crypt.stringToByteArray('abc'));
    assertEquals(
        'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
        crypt.byteArrayToHex(sha256.digest()));

    // NIST multi-block test vector.
    sha256.reset();
    sha256.update(crypt.stringToByteArray(
        'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq'));
    assertEquals(
        '248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1',
        crypt.byteArrayToHex(sha256.digest()));

    // Message larger than one block (but less than two).
    sha256.reset();
    const biggerThanOneBlock = 'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq' +
        'asdfljhr78yasdfljh45opa78sdf' +
        '120839414104897aavnasdfafasd';
    assertTrue(
        biggerThanOneBlock.length > crypt.Sha2.BLOCKSIZE_ &&
        biggerThanOneBlock.length < 2 * crypt.Sha2.BLOCKSIZE_);
    sha256.update(crypt.stringToByteArray(biggerThanOneBlock));
    assertEquals(
        '390a5035433e46b740600f3117d11ece3c64706dc889106666ac04fe4f458abc',
        crypt.byteArrayToHex(sha256.digest()));

    // Message larger than two blocks.
    sha256.reset();
    const biggerThanTwoBlocks = 'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq' +
        'asdfljhr78yasdfljh45opa78sdf' +
        '120839414104897aavnasdfafasd' +
        'laasdouvhalacbnalalseryalcla';
    assertTrue(biggerThanTwoBlocks.length > 2 * crypt.Sha2.BLOCKSIZE_);
    sha256.update(crypt.stringToByteArray(biggerThanTwoBlocks));
    assertEquals(
        'd655c513fd347e9be372d891f8bb42895ca310fabf6ead6681ebc66a04e84db5',
        crypt.byteArrayToHex(sha256.digest()));
  },

  /** Check that the code checks for bad input */
  testBadInput() {
    assertThrows(
        'Bad input',
        /** @suppress {checkTypes} array like isn't a supported type */
        () => {
          new Sha256().update({});
        });
    assertThrows('Floating point not allows', () => {
      new Sha256().update([1, 2, 3, 4, 4.5]);
    });
    assertThrows('Negative not allowed', () => {
      new Sha256().update([1, 2, 3, 4, -10]);
    });
    assertThrows('Must be byte array', () => {
      new Sha256().update([1, 2, 3, 4, {}]);
    });
  },
});

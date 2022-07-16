/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.crypt.Sha224Test');
goog.setTestOnly();

const Sha224 = goog.require('goog.crypt.Sha224');
const crypt = goog.require('goog.crypt');
const hashTester = goog.require('goog.crypt.hashTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const sha224 = new Sha224();
    hashTester.runBasicTests(sha224);
  },

  /** @suppress {visibility} accessing private properties */
  testHashing() {
    // Some test vectors from:
    // csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf

    const sha224 = new Sha224();

    // NIST one block test vector.
    sha224.update(crypt.stringToByteArray('abc'));
    assertEquals(
        '23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7',
        crypt.byteArrayToHex(sha224.digest()));

    // NIST multi-block test vector.
    sha224.reset();
    sha224.update(crypt.stringToByteArray(
        'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq'));
    assertEquals(
        '75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525',
        crypt.byteArrayToHex(sha224.digest()));

    // Message larger than one block (but less than two).
    sha224.reset();
    const biggerThanOneBlock = 'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq' +
        'asdfljhr78yasdfljh45opa78sdf' +
        '120839414104897aavnasdfafasd';
    assertTrue(
        biggerThanOneBlock.length > crypt.Sha2.BLOCKSIZE_ &&
        biggerThanOneBlock.length < 2 * crypt.Sha2.BLOCKSIZE_);
    sha224.update(crypt.stringToByteArray(biggerThanOneBlock));
    assertEquals(
        '27c9b678012becd6891bac653f355b2d26f63132e840644d565f5dac',
        crypt.byteArrayToHex(sha224.digest()));

    // Message larger than two blocks.
    sha224.reset();
    const biggerThanTwoBlocks = 'abcdbcdecdefdefgefghfghighij' +
        'hijkijkljklmklmnlmnomnopnopq' +
        'asdfljhr78yasdfljh45opa78sdf' +
        '120839414104897aavnasdfafasd' +
        'laasdouvhalacbnalalseryalcla';
    assertTrue(biggerThanTwoBlocks.length > 2 * crypt.Sha2.BLOCKSIZE_);
    sha224.update(crypt.stringToByteArray(biggerThanTwoBlocks));
    assertEquals(
        '1c2c1455cc984eef6f25ec9d79b1c661b3794887c3d0b24111ed9803',
        crypt.byteArrayToHex(sha224.digest()));
  },
});

/**
 * @fileoverview
 */
/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.provide('goog.crypt.Ctr');

goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.crypt');
goog.requireType('goog.crypt.BlockCipher');

/**
 * Implementation of Ctr mode for block ciphers.  See
 * http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation
 * #Cipher-block_chaining_.28Ctr.29. for an overview, and
 * http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
 * for the spec.
 *
 * @param {!goog.crypt.BlockCipher} cipher The block cipher to use.
 * @constructor
 * @final
 * @struct
 */
goog.crypt.Ctr = function(cipher) {
  'use strict';
  /**
   * Block cipher.
   * @type {!goog.crypt.BlockCipher}
   * @private
   */
  this.cipher_ = cipher;
};

/**
 * Encrypts a message.
 *
 * @param {!Array<number>|!Uint8Array} plainText Message to encrypt. An array of
 *     bytes. The length does not have to be a multiple of the blocksize.
 * @param {!Array<number>|!Uint8Array} initialVector Initial vector for the Ctr
 *     mode. An array of bytes with the same length as the block size, that
 *     should be not reused when using the same key.
 * @return {!Array<number>} Encrypted message.
 */
goog.crypt.Ctr.prototype.encrypt = function(plainText, initialVector) {
  'use strict';
  goog.asserts.assert(
      initialVector.length == this.cipher_.BLOCK_SIZE,
      'Initial vector must be size of one block.');

  // Copy the IV, so it's not modified.
  var counter = goog.array.clone(initialVector);

  var keyStreamBlock = [];
  var encryptedArray = [];
  var plainTextBlock = [];

  while (encryptedArray.length < plainText.length) {
    keyStreamBlock = this.cipher_.encrypt(counter);
    goog.crypt.Ctr.incrementBigEndianCounter_(counter);

    plainTextBlock = Array.prototype.slice.call(
        plainText, encryptedArray.length,
        encryptedArray.length + this.cipher_.BLOCK_SIZE);
    goog.array.extend(
        encryptedArray,
        goog.crypt.xorByteArray(
            plainTextBlock, keyStreamBlock.slice(0, plainTextBlock.length)));
  }

  return encryptedArray;
};


/**
 * Decrypts a message. In CTR, this is the same as encrypting.
 *
 * @param {!Array<number>|!Uint8Array} cipherText Message to decrypt. The length
 *     does not have to be a multiple of the blocksize.
 * @param {!Array<number>|!Uint8Array} initialVector Initial vector for the Ctr
 *     mode. An array of bytes with the same length as the block size.
 * @return {!Array<number>} Decrypted message.
 */
goog.crypt.Ctr.prototype.decrypt = goog.crypt.Ctr.prototype.encrypt;

/**
 * Increments the big-endian integer represented in counter in-place.
 *
 * @param {!Array<number>|!Uint8Array} counter The array of bytes to modify.
 * @private
 */
goog.crypt.Ctr.incrementBigEndianCounter_ = function(counter) {
  'use strict';
  for (var i = counter.length - 1; i >= 0; i--) {
    var currentByte = counter[i];
    currentByte = (currentByte + 1) & 0xFF;  // Allow wrapping around.
    counter[i] = currentByte;
    if (currentByte != 0) {
      // This iteration hasn't wrapped around, which means there is
      // no carry to add to the next byte.
      return;
    }  // else, repeat with next byte.
  }
};

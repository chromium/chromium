/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Interface definition of a block cipher. A block cipher is a
 * pair of algorithms that implement encryption and decryption of input bytes.
 *
 * @see http://en.wikipedia.org/wiki/Block_cipher
 */

goog.provide('goog.crypt.BlockCipher');



/**
 * Interface definition for a block cipher.
 * @interface
 */
goog.crypt.BlockCipher = function() {};

/**
 * Block size, in bytes.
 * @type {number}
 * @const
 * @public
 */
goog.crypt.BlockCipher.prototype.BLOCK_SIZE;

/**
 * Encrypt a plaintext block.  The implementation may expect (and assert)
 * a particular block length.
 * @param {!Array<number>|!Uint8Array} input Plaintext array of input bytes.
 * @return {!Array<number>} Encrypted ciphertext array of bytes.  Should be the
 *     same length as input.
 */
goog.crypt.BlockCipher.prototype.encrypt;


/**
 * Decrypt a plaintext block.  The implementation may expect (and assert)
 * a particular block length.
 * @param {!Array<number>|!Uint8Array} input Ciphertext. Array of input bytes.
 * @return {!Array<number>} Decrypted plaintext array of bytes.  Should be the
 *     same length as input.
 */
goog.crypt.BlockCipher.prototype.decrypt;

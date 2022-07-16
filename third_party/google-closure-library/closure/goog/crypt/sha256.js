/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview SHA-256 cryptographic hash.
 *
 * Usage:
 *   var sha256 = new goog.crypt.Sha256();
 *   sha256.update(bytes);
 *   var hash = sha256.digest();
 */

goog.provide('goog.crypt.Sha256');

goog.require('goog.crypt.Sha2');



/**
 * SHA-256 cryptographic hash constructor.
 *
 * @constructor
 * @extends {goog.crypt.Sha2}
 * @final
 * @struct
 */
goog.crypt.Sha256 = function() {
  'use strict';
  goog.crypt.Sha256.base(
      this, 'constructor', 8, goog.crypt.Sha256.INIT_HASH_BLOCK_);
};
goog.inherits(goog.crypt.Sha256, goog.crypt.Sha2);


/** @private {!Array<number>} */
goog.crypt.Sha256.INIT_HASH_BLOCK_ = [
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c,
  0x1f83d9ab, 0x5be0cd19
];

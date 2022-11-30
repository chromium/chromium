/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview SHA-224 cryptographic hash.
 *
 * Usage:
 *   var sha224 = new goog.crypt.Sha224();
 *   sha224.update(bytes);
 *   var hash = sha224.digest();
 */

goog.provide('goog.crypt.Sha224');

goog.require('goog.crypt.Sha2');



/**
 * SHA-224 cryptographic hash constructor.
 *
 * @constructor
 * @extends {goog.crypt.Sha2}
 * @final
 * @struct
 */
goog.crypt.Sha224 = function() {
  'use strict';
  goog.crypt.Sha224.base(
      this, 'constructor', 7, goog.crypt.Sha224.INIT_HASH_BLOCK_);
};
goog.inherits(goog.crypt.Sha224, goog.crypt.Sha2);


/** @private {!Array<number>} */
goog.crypt.Sha224.INIT_HASH_BLOCK_ = [
  0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511,
  0x64f98fa7, 0xbefa4fa4
];

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Implementation of HMAC in JavaScript.
 *
 * Usage:
 *   var hmac = new goog.crypt.Hmac(new goog.crypt.sha1(), key, 64);
 *   var digest = hmac.getHmac(bytes);
 */


goog.provide('goog.crypt.Hmac');

goog.require('goog.crypt.Hash');



/**
 * @constructor
 * @param {!goog.crypt.Hash} hasher An object to serve as a hash function.
 * @param {Array<number>} key The secret key to use to calculate the hmac.
 *     Should be an array of not more than `blockSize` integers in
       {0, 255}.
 * @param {number=} opt_blockSize Optional. The block size `hasher` uses.
 *     If not specified, uses the block size from the hasher, or 16 if it is
 *     not specified.
 * @extends {goog.crypt.Hash}
 * @final
 * @struct
 */
goog.crypt.Hmac = function(hasher, key, opt_blockSize) {
  'use strict';
  goog.crypt.Hmac.base(this, 'constructor');

  /**
   * The underlying hasher to calculate hash.
   *
   * @type {!goog.crypt.Hash}
   * @private
   */
  this.hasher_ = hasher;

  this.blockSize = opt_blockSize || hasher.blockSize || 16;

  /**
   * The outer padding array of hmac
   *
   * @type {!Array<number>}
   * @private
   */
  this.keyO_ = new Array(this.blockSize);

  /**
   * The inner padding array of hmac
   *
   * @type {!Array<number>}
   * @private
   */
  this.keyI_ = new Array(this.blockSize);

  this.initialize_(key);
};
goog.inherits(goog.crypt.Hmac, goog.crypt.Hash);


/**
 * Outer padding byte of HMAC algorith, per http://en.wikipedia.org/wiki/HMAC
 *
 * @type {number}
 * @private
 */
goog.crypt.Hmac.OPAD_ = 0x5c;


/**
 * Inner padding byte of HMAC algorith, per http://en.wikipedia.org/wiki/HMAC
 *
 * @type {number}
 * @private
 */
goog.crypt.Hmac.IPAD_ = 0x36;


/**
 * Initializes Hmac by precalculating the inner and outer paddings.
 *
 * @param {Array<number>} key The secret key to use to calculate the hmac.
 *     Should be an array of not more than `blockSize` integers in
       {0, 255}.
 * @private
 */
goog.crypt.Hmac.prototype.initialize_ = function(key) {
  'use strict';
  if (key.length > this.blockSize) {
    this.hasher_.update(key);
    key = this.hasher_.digest();
    this.hasher_.reset();
  }
  // Precalculate padded and xor'd keys.
  var keyByte;
  for (var i = 0; i < this.blockSize; i++) {
    if (i < key.length) {
      keyByte = key[i];
    } else {
      keyByte = 0;
    }
    this.keyO_[i] = keyByte ^ goog.crypt.Hmac.OPAD_;
    this.keyI_[i] = keyByte ^ goog.crypt.Hmac.IPAD_;
  }
  // Be ready for an immediate update.
  this.hasher_.update(this.keyI_);
};


/** @override */
goog.crypt.Hmac.prototype.reset = function() {
  'use strict';
  this.hasher_.reset();
  this.hasher_.update(this.keyI_);
};


/** @override */
goog.crypt.Hmac.prototype.update = function(bytes, opt_length) {
  'use strict';
  this.hasher_.update(bytes, opt_length);
};


/** @override */
goog.crypt.Hmac.prototype.digest = function() {
  'use strict';
  var temp = this.hasher_.digest();
  this.hasher_.reset();
  this.hasher_.update(this.keyO_);
  this.hasher_.update(temp);
  return this.hasher_.digest();
};


/**
 * Calculates an HMAC for a given message.
 *
 * @param {Array<number>|Uint8Array|string} message  Data to Hmac.
 * @return {!Array<number>} the digest of the given message.
 */
goog.crypt.Hmac.prototype.getHmac = function(message) {
  'use strict';
  this.reset();
  this.update(message);
  return this.digest();
};

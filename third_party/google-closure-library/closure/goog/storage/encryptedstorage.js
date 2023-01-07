/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a convenient API for data persistence with key and
 * object encryption. Without a valid secret, the existence of a particular
 * key can't be verified and values can't be decrypted. The value encryption
 * is salted, so subsequent writes of the same cleartext result in different
 * ciphertext. The ciphertext is *not* authenticated, so there is no protection
 * against data manipulation.
 *
 * The metadata is *not* encrypted, so expired keys can be cleaned up without
 * decrypting them. If sensitive metadata is added in subclasses, it is up
 * to the subclass to protect this information, perhaps by embedding it in
 * the object.
 */

goog.provide('goog.storage.EncryptedStorage');

goog.require('goog.crypt');
goog.require('goog.crypt.Arc4');
goog.require('goog.crypt.Sha1');
goog.require('goog.crypt.base64');
goog.require('goog.json');
goog.require('goog.json.Serializer');
goog.require('goog.storage.CollectableStorage');
goog.require('goog.storage.ErrorCode');
goog.require('goog.storage.RichStorage');
goog.requireType('goog.storage.mechanism.IterableMechanism');



/**
 * Provides an encrypted storage. The keys are hashed with a secret, so
 * their existence cannot be verified without the knowledge of the secret.
 * The values are encrypted using the key, a salt, and the secret, so
 * stream cipher initialization varies for each stored value.
 *
 * @param {!goog.storage.mechanism.IterableMechanism} mechanism The underlying
 *     storage mechanism.
 * @param {string} secret The secret key used to encrypt the storage.
 * @constructor
 * @struct
 * @extends {goog.storage.CollectableStorage}
 * @final
 */
goog.storage.EncryptedStorage = function(mechanism, secret) {
  'use strict';
  goog.storage.EncryptedStorage.base(this, 'constructor', mechanism);
  /**
   * The secret used to encrypt the storage.
   *
   * @private {!Array<number>}
   */
  this.secret_ = goog.crypt.stringToByteArray(secret);

  /**
   * The JSON serializer used to serialize values before encryption. This can
   * be potentially different from serializing for the storage mechanism (see
   * goog.storage.Storage), so a separate serializer is kept here.
   *
   * @private {!goog.json.Serializer}
   */
  this.cleartextSerializer_ = new goog.json.Serializer();
};
goog.inherits(goog.storage.EncryptedStorage, goog.storage.CollectableStorage);


/**
 * Metadata key under which the salt is stored.
 *
 * @type {string}
 * @protected
 */
goog.storage.EncryptedStorage.SALT_KEY = 'salt';


/**
 * Hashes a key using the secret.
 *
 * @param {string} key The key.
 * @return {string} The hash.
 * @private
 */
goog.storage.EncryptedStorage.prototype.hashKeyWithSecret_ = function(key) {
  'use strict';
  const sha1 = new goog.crypt.Sha1();
  sha1.update(goog.crypt.stringToByteArray(key));
  sha1.update(this.secret_);
  return goog.crypt.base64.encodeByteArray(
      sha1.digest(), goog.crypt.base64.Alphabet.WEBSAFE_DOT_PADDING);
};


/**
 * Encrypts a value using a key, a salt, and the secret.
 *
 * @param {!Array<number>} salt The salt.
 * @param {string} key The key.
 * @param {string} value The cleartext value.
 * @return {string} The encrypted value.
 * @private
 */
goog.storage.EncryptedStorage.prototype.encryptValue_ = function(
    salt, key, value) {
  'use strict';
  if (!(salt.length > 0)) {
    throw new Error('Non-empty salt must be provided');
  }
  const sha1 = new goog.crypt.Sha1();
  sha1.update(goog.crypt.stringToByteArray(key));
  sha1.update(salt);
  sha1.update(this.secret_);
  const arc4 = new goog.crypt.Arc4();
  arc4.setKey(sha1.digest());
  // Warm up the streamcypher state, see goog.crypt.Arc4 for details.
  arc4.discard(1536);
  const bytes = goog.crypt.stringToByteArray(value);
  arc4.crypt(bytes);
  return goog.crypt.byteArrayToString(bytes);
};


/**
 * Decrypts a value using a key, a salt, and the secret.
 *
 * @param {!Array<number>} salt The salt.
 * @param {string} key The key.
 * @param {string} value The encrypted value.
 * @return {string} The decrypted value.
 * @private
 */
goog.storage.EncryptedStorage.prototype.decryptValue_ = function(
    salt, key, value) {
  'use strict';
  // ARC4 is symmetric.
  return this.encryptValue_(salt, key, value);
};


/** @override */
goog.storage.EncryptedStorage.prototype.set = function(
    key, value, opt_expiration) {
  'use strict';
  if (value === undefined) {
    goog.storage.EncryptedStorage.prototype.remove.call(this, key);
    return;
  }
  const salt = [];
  // 64-bit random salt.
  for (let i = 0; i < 8; ++i) {
    salt[i] = Math.floor(Math.random() * 0x100);
  }
  const wrapper = new goog.storage.RichStorage.Wrapper(this.encryptValue_(
      salt, key, this.cleartextSerializer_.serialize(value)));
  wrapper[goog.storage.EncryptedStorage.SALT_KEY] = salt;
  goog.storage.EncryptedStorage.base(
      this, 'set', this.hashKeyWithSecret_(key), wrapper, opt_expiration);
};


/** @override */
goog.storage.EncryptedStorage.prototype.getWrapper = function(
    key, opt_expired) {
  'use strict';
  const wrapper = goog.storage.EncryptedStorage.base(
      this, 'getWrapper', this.hashKeyWithSecret_(key), opt_expired);
  if (!wrapper) {
    return undefined;
  }
  const value = goog.storage.RichStorage.Wrapper.unwrap(wrapper);
  const salt = wrapper[goog.storage.EncryptedStorage.SALT_KEY];
  if (typeof value !== 'string' || !Array.isArray(salt) || !salt.length) {
    throw goog.storage.ErrorCode.INVALID_VALUE;
  }
  const json = this.decryptValue_(salt, key, value);

  try {
    wrapper[goog.storage.RichStorage.DATA_KEY] = JSON.parse(json);
  } catch (e) {
    throw goog.storage.ErrorCode.DECRYPTION_ERROR;
  }
  return wrapper;
};


/** @override */
goog.storage.EncryptedStorage.prototype.remove = function(key) {
  'use strict';
  goog.storage.EncryptedStorage.base(
      this, 'remove', this.hashKeyWithSecret_(key));
};

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.EncryptedStorageTest');
goog.setTestOnly();

const EncryptedStorage = goog.require('goog.storage.EncryptedStorage');
const ErrorCode = goog.require('goog.storage.ErrorCode');
const FakeMechanism = goog.require('goog.testing.storage.FakeMechanism');
const MockClock = goog.require('goog.testing.MockClock');
const PseudoRandom = goog.require('goog.testing.PseudoRandom');
const RichStorage = goog.require('goog.storage.RichStorage');
const collectableStorageTester = goog.require('goog.storage.collectableStorageTester');
const googJson = goog.require('goog.json');
const storageTester = goog.require('goog.storage.storageTester');
const testSuite = goog.require('goog.testing.testSuite');

function getEncryptedWrapper(storage, key) {
  return JSON.parse(storage.mechanism.get(storage.hashKeyWithSecret_(key)));
}

/** @suppress {visibility} suppression added to enable type checking */
function getEncryptedData(storage, key) {
  return getEncryptedWrapper(storage, key)[RichStorage.DATA_KEY];
}

/** @suppress {visibility} suppression added to enable type checking */
function decryptWrapper(storage, key, wrapper) {
  return JSON.parse(storage.decryptValue_(
      wrapper[EncryptedStorage.SALT_KEY], key, wrapper[RichStorage.DATA_KEY]));
}

function hammingDistance(a, b) {
  if (a.length != b.length) {
    throw new Error('Lengths must be the same for Hamming distance');
  }
  let distance = 0;
  for (let i = 0; i < a.length; ++i) {
    if (a.charAt(i) != b.charAt(i)) {
      ++distance;
    }
  }
  return distance;
}

testSuite({
  testBasicOperations() {
    const mechanism = new FakeMechanism();
    const storage = new EncryptedStorage(mechanism, 'secret');
    storageTester.runBasicTests(storage);
  },

  testExpiredKeyCollection() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new EncryptedStorage(mechanism, 'secret');

    collectableStorageTester.runBasicTests(mechanism, clock, storage);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testEncryption() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new EncryptedStorage(mechanism, 'secret');
    const mallory = new EncryptedStorage(mechanism, 'guess');

    // Simple Objects.
    storage.set('first', 'Hello world!');
    storage.set('second', ['one', 'two', 'three'], 1000);
    storage.set('third', {'a': 97, 'b': 98});

    // Wrong secret can't find keys.
    assertNull(mechanism.get('first'));
    assertNull(mechanism.get('second'));
    assertNull(mechanism.get('third'));
    assertUndefined(mallory.get('first'));
    assertUndefined(mallory.get('second'));
    assertUndefined(mallory.get('third'));

    // Wrong secret can't overwrite keys.
    mallory.set('first', 'Ho ho ho!');
    assertObjectEquals('Ho ho ho!', mallory.get('first'));
    assertObjectEquals('Hello world!', storage.get('first'));
    mallory.remove('first');

    // Correct key decrypts properly.
    assertObjectEquals('Hello world!', storage.get('first'));
    assertObjectEquals(['one', 'two', 'three'], storage.get('second'));
    assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));

    // Wrong secret can't decode values even if the key is revealed.
    const encryptedWrapper = getEncryptedWrapper(storage, 'first');
    assertObjectEquals(
        'Hello world!', decryptWrapper(storage, 'first', encryptedWrapper));
    assertThrows(() => {
      decryptWrapper(mallory, 'first', encryptedWrapper);
    });

    // If the value is overwritten, it can't be decrypted.
    /** @suppress {visibility} suppression added to enable type checking */
    encryptedWrapper[RichStorage.DATA_KEY] = 'kaboom';
    mechanism.set(
        storage.hashKeyWithSecret_('first'),
        googJson.serialize(encryptedWrapper));
    assertEquals(ErrorCode.DECRYPTION_ERROR, assertThrows(() => {
                   storage.get('first');
                 }));

    // Test garbage collection.
    storage.collect();
    assertNotNull(getEncryptedWrapper(storage, 'first'));
    assertObjectEquals(['one', 'two', 'three'], storage.get('second'));
    assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));
    clock.tick(2000);
    storage.collect();
    assertNotNull(getEncryptedWrapper(storage, 'first'));
    assertUndefined(storage.get('second'));
    assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));
    mechanism.set(storage.hashKeyWithSecret_('first'), '"kaboom"');
    storage.collect();
    assertNotNull(getEncryptedWrapper(storage, 'first'));
    assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));
    storage.collect(true);
    assertUndefined(storage.get('first'));
    assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));

    // Clean up.
    storage.remove('third');
    assertUndefined(storage.get('third'));
    clock.uninstall();
  },

  testSalting() {
    const mechanism = new FakeMechanism();
    const randomMock = new PseudoRandom(0, true);
    const storage = new EncryptedStorage(mechanism, 'secret');

    // Same value under two different keys should appear very different,
    // even with the same salt.
    storage.set('one', 'Hello world!');
    randomMock.seed(0);  // Reset the generator so we get the same salt.
    storage.set('two', 'Hello world!');
    const golden = getEncryptedData(storage, 'one');
    assertRoughlyEquals(
        'Ciphertext did not change with keys', golden.length,
        hammingDistance(golden, getEncryptedData(storage, 'two')), 2);

    // Same key-value pair written second time should appear very different.
    storage.set('one', 'Hello world!');
    assertRoughlyEquals(
        'Salting seems to have failed', golden.length,
        hammingDistance(golden, getEncryptedData(storage, 'one')), 2);

    // Clean up.
    storage.remove('1');
    storage.remove('2');
    randomMock.uninstall();
  },
});

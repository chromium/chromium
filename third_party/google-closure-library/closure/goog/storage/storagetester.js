/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Helper for various storage tests.
 */

goog.provide('goog.storage.storageTester');
goog.setTestOnly();

goog.require('goog.storage.Storage');
goog.require('goog.structs.Map');
goog.require('goog.testing.asserts');

/**
 * @param {!goog.storage.Storage} storage
 */
goog.storage.storageTester.runBasicTests = function(storage) {
  'use strict';
  // Simple Objects.
  storage.set('first', 'Hello world!');
  storage.set('second', ['one', 'two', 'three']);
  storage.set('third', {'a': 97, 'b': 98});
  assertEquals('Hello world!', storage.get('first'));
  assertObjectEquals(['one', 'two', 'three'], storage.get('second'));
  assertObjectEquals({'a': 97, 'b': 98}, storage.get('third'));

  // Some more complex fun with a Map.
  const map = new goog.structs.Map();
  map.set('Alice', 'Hello world!');
  map.set('Bob', ['one', 'two', 'three']);
  map.set('Cecile', {'a': 97, 'b': 98});
  storage.set('first', map.toObject());
  assertObjectEquals(map.toObject(), storage.get('first'));

  // Setting weird values.
  storage.set('second', null);
  assertEquals(null, storage.get('second'));
  storage.set('second', undefined);
  assertEquals(undefined, storage.get('second'));
  storage.set('second', '');
  assertEquals('', storage.get('second'));

  // Clean up.
  storage.remove('first');
  storage.remove('second');
  storage.remove('third');
  assertUndefined(storage.get('first'));
  assertUndefined(storage.get('second'));
  assertUndefined(storage.get('third'));
};

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.MockStorageTest');
goog.setTestOnly();

const MockStorage = goog.require('goog.testing.MockStorage');
const testSuite = goog.require('goog.testing.testSuite');

let instance;

testSuite({
  setUp() {
    instance = new MockStorage();
  },

  /** Tests the MockStorage interface. */
  testMockStorage() {
    assertEquals(0, instance.length);

    instance.setItem('foo', 'bar');
    assertEquals(1, instance.length);
    assertEquals('bar', instance.getItem('foo'));
    assertEquals('foo', instance.key(0));

    instance.setItem('foo', 'baz');
    assertEquals('baz', instance.getItem('foo'));

    instance.setItem('goo', 'gl');
    assertEquals(2, instance.length);
    assertEquals('gl', instance.getItem('goo'));
    assertEquals('goo', instance.key(1));

    assertNull(instance.getItem('poogle'));

    instance.removeItem('foo');
    assertEquals(1, instance.length);
    assertEquals('goo', instance.key(0));

    instance.setItem('a', 12);
    assertEquals('12', instance.getItem('a'));
    instance.setItem('b', false);
    assertEquals('false', instance.getItem('b'));
    instance.setItem('c', {a: 1, b: 12});
    assertEquals('[object Object]', instance.getItem('c'));

    instance.clear();
    assertEquals(0, instance.length);

    // Test some special cases.
    instance.setItem('emptyString', '');
    assertEquals('', instance.getItem('emptyString'));
    instance.setItem('isNull', null);
    assertEquals('null', instance.getItem('isNull'));
    instance.setItem('isUndefined', undefined);
    assertEquals('undefined', instance.getItem('isUndefined'));
    instance.setItem('', 'empty key');
    assertEquals('empty key', instance.getItem(''));
    assertEquals(4, instance.length);
  },
});

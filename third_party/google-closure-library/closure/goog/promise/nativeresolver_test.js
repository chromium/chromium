/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.promise.nativeResolverTest');
goog.setTestOnly();

const NativeResolver = goog.require('goog.promise.NativeResolver');
const testSuite = goog.require('goog.testing.testSuite');

let resolver;

testSuite({
  setUp() {
    resolver = new NativeResolver();
  },

  testResolve() {
    resolver.resolve('test');
    return resolver.promise.then((val) => {
      assertEquals('test', val);
    }, fail);
  },

  testReject() {
    resolver.reject(new Error('test'));
    return resolver.promise.then(fail, (e) => {
      assertEquals('test', e.message);
    });
  }
});

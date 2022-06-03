/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.ListenableTest');
goog.setTestOnly();

const Listenable = goog.require('goog.events.Listenable');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testIsImplementedBy() {
    const ListenableClass = class {};
    Listenable.addImplementation(ListenableClass);

    const NonListenableClass = class {};

    assertTrue(Listenable.isImplementedBy(new ListenableClass()));
    assertFalse(Listenable.isImplementedBy(new NonListenableClass()));
  },
});

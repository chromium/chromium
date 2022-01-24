/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.mechanismfactoryTest');
goog.setTestOnly();

const mechanismfactory = goog.require('goog.storage.mechanism.mechanismfactory');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testAvailability() {
    let mechanism = mechanismfactory.create('test');
    let mechanism_shared = mechanismfactory.create('test');
    let mechanism_separate = mechanismfactory.create('test2');

    const probe = mechanismfactory.create();
    if (!!probe) {
      assertNotNull(mechanism);
      assertNotNull(mechanism_shared);
      assertNotNull(mechanism_separate);
    }

    if (!!mechanism) {
      mechanism.clear();
      mechanism = null;
    }
    if (!!mechanism_shared) {
      mechanism_shared.clear();
      mechanism_shared = null;
    }
    if (!!mechanism_separate) {
      mechanism_separate.clear();
      mechanism_separate = null;
    }
  },
});

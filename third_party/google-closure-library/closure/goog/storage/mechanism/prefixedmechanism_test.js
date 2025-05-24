/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.PrefixedMechanismTest');
goog.setTestOnly();

const HTML5LocalStorage = goog.require('goog.storage.mechanism.HTML5LocalStorage');
const PrefixedMechanism = goog.require('goog.storage.mechanism.PrefixedMechanism');
const iterableMechanismTests = goog.require('goog.storage.mechanism.iterableMechanismTests');
const mechanismSeparationTests = goog.require('goog.storage.mechanism.mechanismSeparationTests');
const mechanismSharingTests = goog.require('goog.storage.mechanism.mechanismSharingTests');
const mechanismTests = goog.require('goog.storage.mechanism.mechanismTests');
const testSuite = goog.require('goog.testing.testSuite');

let submechanism = null;
let mechanism;
let mechanismShared;
let mechanismSeparate;

testSuite({
  setUp() {
    submechanism = new HTML5LocalStorage();
    if (submechanism.isAvailable()) {
      mechanism = new PrefixedMechanism(submechanism, 'test');
      mechanismShared = new PrefixedMechanism(submechanism, 'test');
      mechanismSeparate = new PrefixedMechanism(submechanism, 'test2');
    }
  },

  tearDown() {
    if (!!mechanism) {
      mechanism.clear();
      mechanism = null;
    }
    if (!!mechanismShared) {
      mechanismShared.clear();
      mechanismShared = null;
    }
    if (!!mechanismSeparate) {
      mechanismSeparate.clear();
      mechanismSeparate = null;
    }
  },

  testAvailability() {
    if (submechanism.isAvailable()) {
      assertNotNull(mechanism);
      assertNotNull(mechanismShared);
      assertNotNull(mechanismSeparate);
    }
  },

  ...mechanismTests.register({
    getMechanism: function() {
      return mechanism;
    },
    getMinimumQuota: function() {
      return 0;
    },
  }),

  ...iterableMechanismTests.register({
    getMechanism: function() {
      return mechanism;
    },
  }),

  ...mechanismSharingTests.register({
    getMechanism: function() {
      return mechanism;
    },
    getMechanismShared: function() {
      return mechanismShared;
    },
  }),

  ...mechanismSeparationTests.register({
    getMechanism: function() {
      return mechanism;
    },
    getMechanismSeparate: function() {
      return mechanismSeparate;
    },
  }),
});

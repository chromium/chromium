/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.async.AnimationDelayTest');
goog.setTestOnly('goog.async.AnimationDelayTest');

const AnimationDelay = goog.require('goog.async.AnimationDelay');
const Promise = goog.require('goog.Promise');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Timer = goog.require('goog.Timer');
const testSuite = goog.require('goog.testing.testSuite');

const TEST_DELAY = 50;
const stubs = new PropertyReplacer();

testSuite({
  tearDown: function() {
    stubs.reset();
  },

  testStart: function() {
    let resolver = Promise.withResolver();
    const start = goog.now();
    const delay = new AnimationDelay(function(end) {
      assertNotNull(resolver);  // fail if called multiple times
      resolver.resolve();
      resolver = null;
    });

    delay.start();

    return resolver.promise;
  },

  testStop: function() {
    const resolver = Promise.withResolver();
    const start = goog.now();
    const delay = new AnimationDelay(function(end) {
      resolver.reject();
    });

    delay.start();
    delay.stop();

    return Timer.promise(TEST_DELAY).then(function() {
      resolver.resolve();
      return resolver.promise;
    });
  },

  testAlwaysUseGoogNowForHandlerTimestamp: function() {
    const resolver = Promise.withResolver();
    const expectedValue = 12345.1;
    stubs.set(goog, 'now', function() { return expectedValue; });

    const delay = new AnimationDelay(function(timestamp) {
      assertEquals(expectedValue, timestamp);
      resolver.resolve();
    });

    delay.start();

    return resolver.promise;
  },

  testStartIfActive: function() {
    const delay = new AnimationDelay(goog.nullFunction);
    delay.start();

    let startWasCalled = false;
    stubs.set(AnimationDelay.prototype, 'start', function() {
      startWasCalled = true;
    });

    delay.startIfNotActive();
    assertEquals(startWasCalled, false);
    delay.stop();
    delay.startIfNotActive();
    assertEquals(startWasCalled, true);
  }
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.async.DelayTest');
goog.setTestOnly();

const Delay = goog.require('goog.async.Delay');
const MockClock = goog.require('goog.testing.MockClock');
const testSuite = goog.require('goog.testing.testSuite');

let invoked = false;
let delay = null;
let clock = null;

function callback() {
  invoked = true;
}

testSuite({
  setUp() {
    clock = new MockClock(true);
    invoked = false;
    delay = new Delay(callback, 200);
  },

  tearDown() {
    clock.dispose();
    delay.dispose();
  },

  testDelay() {
    delay.start();
    assertFalse(invoked);

    clock.tick(100);
    assertFalse(invoked);

    clock.tick(100);
    assertTrue(invoked);
  },

  testStop() {
    delay.start();

    clock.tick(100);
    assertFalse(invoked);

    delay.stop();
    clock.tick(100);
    assertFalse(invoked);
  },

  testIsActive() {
    assertFalse(delay.isActive());
    delay.start();
    assertTrue(delay.isActive());
    clock.tick(200);
    assertFalse(delay.isActive());
  },

  testRestart() {
    delay.start();
    clock.tick(100);

    delay.stop();
    assertFalse(invoked);

    delay.start();
    clock.tick(199);
    assertFalse(invoked);

    clock.tick(1);
    assertTrue(invoked);

    invoked = false;
    delay.start();
    clock.tick(200);
    assertTrue(invoked);
  },

  testStartIfNotActive() {
    delay.startIfNotActive();
    clock.tick(100);

    delay.stop();
    assertFalse(invoked);

    delay.startIfNotActive();
    clock.tick(199);
    assertFalse(invoked);

    clock.tick(1);
    assertTrue(invoked);

    invoked = false;
    delay.start();
    clock.tick(199);

    assertFalse(invoked);

    delay.startIfNotActive();
    clock.tick(1);

    assertTrue(invoked);
  },

  testOverride() {
    delay.start(50);
    clock.tick(49);
    assertFalse(invoked);

    clock.tick(1);
    assertTrue(invoked);
  },

  testDispose() {
    delay.start();
    delay.dispose();
    assertTrue(delay.isDisposed());

    clock.tick(500);
    assertFalse(invoked);
  },

  testFire() {
    delay.start();

    clock.tick(50);
    delay.fire();
    assertTrue(invoked);
    assertFalse(delay.isActive());

    invoked = false;
    clock.tick(200);
    assertFalse(
        'Delay fired early with fire call, timeout should have been ' +
            'cleared',
        invoked);
  },

  testFireIfActive() {
    delay.fireIfActive();
    assertFalse(invoked);

    delay.start();
    delay.fireIfActive();
    assertTrue(invoked);
    invoked = false;
    clock.tick(300);
    assertFalse(
        'Delay fired early with fireIfActive, timeout should have been ' +
            'cleared',
        invoked);
  },
});

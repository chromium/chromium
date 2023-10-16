// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {Speedometer} from './file_operation_util.js';

/**
 * Mock JS Date.
 *
 * The stop() method should be called to restore original Date.
 */
class MockDate {
  constructor() {
    this.originalNow = Date.now;
    this.tick_ = 0;
    Date.now = this.now.bind(this);
  }

  /**
   * Increases current timestamp.
   *
   * @param {number} msec Milliseconds to add to the current timestamp.
   */
  tick(msec) {
    this.tick_ += msec;
  }

  /**
   * @returns {number} Current timestamp of the mock object.
   */
  now() {
    return this.tick_;
  }

  /**
   * Restore original Date.now method.
   */
  stop() {
    Date.now = this.originalNow;
  }
}

/**
 * Tests Speedometer's speed calculations.
 */
export function testSpeedometerMovingAverage() {
  const speedometer = new Speedometer();
  const mockDate = new MockDate();

  speedometer.setTotalBytes(2000);

  // Time elapsed before the first sample shouldn't have any impact.
  mockDate.tick(10000);

  assertEquals(0, speedometer.getSampleCount());
  assertTrue(isNaN(speedometer.getRemainingTime()));

  // 1st sample, t=0s.
  mockDate.tick(1000);
  speedometer.update(100);

  assertEquals(1, speedometer.getSampleCount());
  assertTrue(isNaN(speedometer.getRemainingTime()));

  // Sample received less than a second after the previous one should be
  // ignored.
  mockDate.tick(999);
  speedometer.update(300);

  assertEquals(1, speedometer.getSampleCount());
  assertTrue(isNaN(speedometer.getRemainingTime()));

  // From 2 samples, the average and the current speed can be computed.
  // 2nd sample, t=1s.
  mockDate.tick(1);
  speedometer.update(300);

  assertEquals(2, speedometer.getSampleCount());
  assertEquals(9, Math.round(speedometer.getRemainingTime()));

  // 3rd sample, t=2s.
  mockDate.tick(1000);
  speedometer.update(300);

  assertEquals(3, speedometer.getSampleCount());
  assertEquals(17, Math.round(speedometer.getRemainingTime()));

  // 4th sample, t=4s.
  mockDate.tick(2000);
  speedometer.update(300);

  assertEquals(4, speedometer.getSampleCount());
  assertEquals(42, Math.round(speedometer.getRemainingTime()));

  // 5th sample, t=5s.
  mockDate.tick(1000);
  speedometer.update(600);

  assertEquals(5, speedometer.getSampleCount());
  assertEquals(20, Math.round(speedometer.getRemainingTime()));

  // Elapsed time should be taken in account by getRemainingTime().
  mockDate.tick(12000);
  assertEquals(8, Math.round(speedometer.getRemainingTime()));

  // getRemainingTime() can return a negative value.
  mockDate.tick(12000);
  assertEquals(-4, Math.round(speedometer.getRemainingTime()));

  mockDate.stop();
}

/**
 * Tests Speedometer's sample queue.
 */
export function testSpeedometerBufferRing() {
  const maxSamples = 20;
  const speedometer = new Speedometer(maxSamples);
  const mockDate = new MockDate();

  speedometer.setTotalBytes(20000);

  // Slow speed of 100 bytes per second.
  for (let i = 0; i < maxSamples; i++) {
    assertEquals(i, speedometer.getSampleCount());
    mockDate.tick(1000);
    speedometer.update(i * 100);
  }

  assertEquals(maxSamples, speedometer.getSampleCount());
  assertEquals(181, Math.round(speedometer.getRemainingTime()));

  // Faster speed of 300 bytes per second.
  for (let i = 0; i < maxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    assertEquals(maxSamples, speedometer.getSampleCount());
    mockDate.tick(1000);
    speedometer.update(2100 + i * 300);

    // Current speed should be seen as accelerating, thus the remaining time
    // decreasing.
    assertGT(181, Math.round(speedometer.getRemainingTime()));
  }

  assertEquals(maxSamples, speedometer.getSampleCount());
  assertEquals(41, Math.round(speedometer.getRemainingTime()));

  // Stalling.
  for (let i = 0; i < maxSamples; i++) {
    // Check buffer not expanded more than the specified length.
    assertEquals(maxSamples, speedometer.getSampleCount());
    mockDate.tick(1000);
    speedometer.update(7965);
  }

  assertEquals(maxSamples, speedometer.getSampleCount());

  // getRemainingTime() can return an infinite value.
  assertEquals(Infinity, Math.round(speedometer.getRemainingTime()));

  mockDate.stop();
}

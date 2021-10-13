/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.ExponentialBackoffTest');
goog.setTestOnly();

const ExponentialBackoff = goog.require('goog.math.ExponentialBackoff');
const testSuite = goog.require('goog.testing.testSuite');

const INITIAL_VALUE = 1;

const MAX_VALUE = 10;

function assertValueAndCounts(value, backoffCount, decayCount, backoff) {
  assertEquals('Wrong value', value, backoff.getValue());
  assertEquals('Wrong backoff count', backoffCount, backoff.getBackoffCount());
  assertEquals('Wrong decay count', decayCount, backoff.getDecayCount());
}

function assertValueAndBackoffCount(value, count, backoff) {
  assertEquals('Wrong value', value, backoff.getValue());
  assertEquals('Wrong backoff count', count, backoff.getBackoffCount());
}

function assertValueAndDecayCount(value, count, backoff) {
  assertEquals('Wrong value', value, backoff.getValue());
  assertEquals('Wrong decay count', count, backoff.getDecayCount());
}

function assertValueRangeAndBackoffCount(
    minBackoffValue, maxBackoffValue, count, backoff) {
  assertTrue('Value too small', backoff.getValue() >= minBackoffValue);
  assertTrue('Value too large', backoff.getValue() <= maxBackoffValue);
  assertEquals('Wrong backoff count', count, backoff.getBackoffCount());
}

function assertValueRangeAndDecayCount(
    minBackoffValue, maxBackoffValue, count, backoff) {
  assertTrue('Value too small', backoff.getValue() >= minBackoffValue);
  assertTrue('Value too large', backoff.getValue() <= maxBackoffValue);
  assertEquals('Wrong decay count', count, backoff.getDecayCount());
}

function getMinBackoff(baseValue, randomFactor) {
  return Math.round(baseValue - baseValue * randomFactor);
}

function getMaxBackoff(baseValue, randomFactor) {
  return Math.round(baseValue + baseValue * randomFactor);
}

function createBackoff() {
  return new ExponentialBackoff(INITIAL_VALUE, MAX_VALUE);
}

testSuite({
  testInitialState() {
    const backoff = createBackoff();
    assertValueAndCounts(INITIAL_VALUE, 0, 0, backoff);
  },

  testBackoff() {
    const backoff = createBackoff();
    backoff.backoff();
    assertValueAndBackoffCount(2 /* value */, 1 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(4 /* value */, 2 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(8 /* value */, 3 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(MAX_VALUE, 4 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(MAX_VALUE, 5 /* count */, backoff);
  },

  testReset() {
    const backoff = createBackoff();
    backoff.backoff();
    backoff.decay();
    backoff.reset();
    assertValueAndCounts(
        INITIAL_VALUE, 0 /* backoff count */, 0 /* decay count */, backoff);
    backoff.backoff();
    assertValueAndCounts(
        2 /* value */, 1 /* backoff count */, 0 /* decay count */, backoff);
    backoff.decay();
    assertValueAndCounts(
        INITIAL_VALUE, 1 /* backoff count */, 1 /* decay count */, backoff);
  },

  testRandomFactorBackoff() {
    const initialValue = 1;
    const maxValue = 20;
    const randomFactor = 0.5;
    const backoff =
        new ExponentialBackoff(initialValue, maxValue, randomFactor);

    assertValueAndBackoffCount(
        initialValue /* value */, 0 /* count */, backoff);
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(2, randomFactor), getMaxBackoff(2, randomFactor),
        1 /* count */, backoff);
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(4, randomFactor), getMaxBackoff(4, randomFactor),
        2 /* count */, backoff);
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(8, randomFactor), getMaxBackoff(8, randomFactor),
        3 /* count */, backoff);
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(16, randomFactor), maxValue /* max backoff value */,
        4 /* count */, backoff);
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(maxValue, randomFactor), maxValue /* max backoff value */,
        5 /* count */, backoff);
  },

  testRandomFactorDecay() {
    const initialValue = 1;
    const maxValue = 8;
    const randomFactor = 0.5;
    const backoff =
        new ExponentialBackoff(initialValue, maxValue, randomFactor);

    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    assertValueRangeAndBackoffCount(
        getMinBackoff(maxValue, randomFactor), maxValue /* max backoff value */,
        5 /* count */, backoff);
    backoff.decay();
    assertValueRangeAndDecayCount(
        getMinBackoff(4, randomFactor), getMaxBackoff(4, randomFactor),
        1 /* count */, backoff);
    backoff.decay();
    assertValueRangeAndDecayCount(
        getMinBackoff(2, randomFactor), getMaxBackoff(2, randomFactor),
        2 /* count */, backoff);
    backoff.decay();
    assertValueRangeAndDecayCount(
        initialValue, getMaxBackoff(initialValue, randomFactor), 3 /* count */,
        backoff);
  },

  testBackoffFactor() {
    const initialValue = 1;
    const maxValue = 30;
    const randomFactor = undefined;
    const backoffFactor = 3;
    const backoff = new ExponentialBackoff(
        initialValue, maxValue, randomFactor, backoffFactor);

    backoff.backoff();
    assertValueAndBackoffCount(3 /* value */, 1 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(9 /* value */, 2 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(27 /* value */, 3 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(maxValue, 4 /* count */, backoff);
    backoff.backoff();
    assertValueAndBackoffCount(maxValue, 5 /* count */, backoff);
  },

  testDecayFactor() {
    const initialValue = 1;
    const maxValue = 27;
    const randomFactor = undefined;
    const backoffFactor = undefined;
    const decayFactor = 3;
    const backoff = new ExponentialBackoff(
        initialValue, maxValue, randomFactor, backoffFactor, decayFactor);

    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    backoff.backoff();
    assertValueAndCounts(
        maxValue, 5 /* backoff count */, 0 /* decay count */, backoff);
    backoff.decay();
    assertValueAndDecayCount(9, 1 /* count */, backoff);
    backoff.decay();
    assertValueAndDecayCount(3, 2 /* count */, backoff);
    backoff.decay();
    assertValueAndDecayCount(initialValue, 3 /* count */, backoff);
    backoff.decay();
    assertValueAndDecayCount(initialValue, 4 /* count */, backoff);
  },
});

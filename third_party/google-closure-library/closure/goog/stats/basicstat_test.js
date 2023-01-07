/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.stats.BasicStatTest');
goog.setTestOnly();

const BasicStat = goog.require('goog.stats.BasicStat');
const PseudoRandom = goog.require('goog.testing.PseudoRandom');
const format = goog.require('goog.string.format');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/**
 * A horribly inefficient implementation of BasicStat that stores
 * every event in an array and dynamically filters to perform
 * aggregations.
 */
class PerfectlySlowStat {
  constructor(interval) {
    this.interval_ = interval;
    /** @suppress {visibility} suppression added to enable type checking */
    this.slotSize_ = Math.floor(interval / BasicStat.NUM_SLOTS_);
    this.events_ = [];
  }

  incBy(amt, now) {
    this.events_.push({'time': now, 'count': amt});
  }

  getStats(now) {
    const end =
        Math.floor(now / this.slotSize_) * this.slotSize_ + this.slotSize_;
    const start = end - this.interval_;
    const events = this.events_.filter(e => e.time >= start);
    return {
      'count': events.reduce((sum, e) => sum + e.count, 0),
      'min':
          events.reduce((min, e) => Math.min(min, e.count), Number.MAX_VALUE),
      'max':
          events.reduce((max, e) => Math.max(max, e.count), Number.MIN_VALUE),
    };
  }
}

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testGetSlotBoundary() {
    const stat = new BasicStat(1654);
    assertEquals('Checking interval', 33, stat.slotInterval_);

    assertEquals(132, stat.getSlotBoundary_(125));
    assertEquals(165, stat.getSlotBoundary_(132));
    assertEquals(132, stat.getSlotBoundary_(99));
    assertEquals(99, stat.getSlotBoundary_(98));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCheckForTimeTravel() {
    const stat = new BasicStat(1000);

    // no slots yet, should always be OK
    stat.checkForTimeTravel_(100);
    stat.checkForTimeTravel_(-1);

    stat.incBy(1, 125);  // creates a first bucket, ending at t=140

    // Even though these go backwards in time, our basic fuzzy check passes
    // because we just check that the time is within the latest interval bucket.
    stat.checkForTimeTravel_(141);
    stat.checkForTimeTravel_(140);
    stat.checkForTimeTravel_(139);
    stat.checkForTimeTravel_(125);
    stat.checkForTimeTravel_(124);
    stat.checkForTimeTravel_(120);

    // State should still be the same, all of the above times are valid.
    assertEquals(
        'State unchanged when called with good times', 1, stat.get(125));

    stat.checkForTimeTravel_(119);
    assertEquals('Reset after called with a bad time', 0, stat.get(125));
  },

  testConstantIncrementPerSlot() {
    const stat = new BasicStat(1000);

    let now = 1000;
    for (let i = 0; i < 50; ++i) {
      const newMax = 1000 + i;
      const newMin = 1000 - i;
      stat.incBy(newMin, now);
      stat.incBy(newMax, now);

      const msg =
          format('now=%d i=%d newMin=%d newMax=%d', now, i, newMin, newMax);
      assertEquals(msg, 2000 * (i + 1), stat.get(now));
      assertEquals(msg, newMax, stat.getMax(now));
      assertEquals(msg, newMin, stat.getMin(now));

      now += 20;  // push into the next slots
    }

    // The next increment should cause old data to fall off.
    stat.incBy(1, now);
    assertEquals(2000 * 49 + 1, stat.get(now));
    assertEquals(1, stat.getMin(now));
    assertEquals(1049, stat.getMax(now));

    now += 20;  // drop off another bucket
    stat.incBy(1, now);
    assertEquals(2000 * 48 + 2, stat.get(now));
    assertEquals(1, stat.getMin(now));
    assertEquals(1049, stat.getMax(now));
  },

  testSparseBuckets() {
    const stat = new BasicStat(1000);
    let now = 1000;

    stat.incBy(10, now);
    assertEquals(10, stat.get(now));

    now += 5000;  // the old slot is now still in memory, but should be ignored
    stat.incBy(1, now);
    assertEquals(1, stat.get(now));
  },

  testFuzzy() {
    const stat = new BasicStat(1000);
    const test = new PerfectlySlowStat(1000);
    const rand = new PseudoRandom(58849020);
    let eventCount = 0;

    // test over 5 simulated seconds (2 for IE, due to timeouts)
    const simulationDuration = userAgent.IE ? 2000 : 5000;
    for (let now = 1000; now < simulationDuration;) {
      const count = Math.floor(rand.random() * 2147483648);
      const delay = Math.floor(rand.random() * 25);
      for (let i = 0; i <= delay; ++i) {
        const time = now + i;
        const expected = test.getStats(now + i);
        assertEquals(expected.count, stat.get(time));
        assertEquals(expected.min, stat.getMin(time));
        assertEquals(expected.max, stat.getMax(time));
      }

      now += delay;
      stat.incBy(count, now);
      test.incBy(count, now);
      eventCount++;
    }
  },
});

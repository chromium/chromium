/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.PriorityPoolTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const PriorityPool = goog.require('goog.structs.PriorityPool');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

// Implementation of the Pool class with isObjectDead() always returning TRUE,
// so that the Pool will not reuse any objects.
class NoObjectReusePriorityPool extends PriorityPool {
  constructor(min = undefined, max = undefined) {
    super(min, max);
  }

  objectCanBeReused(obj) {
    return false;
  }
}

testSuite({
  testExceedMax1() {
    const p = new PriorityPool(0, 3);

    let getCount1 = 0;
    const callback1 = (obj) => {
      assertNotNull(obj);
      getCount1++;
    };

    let getCount2 = 0;
    const callback2 = (obj) => {
      getCount2++;
    };

    p.getObject(callback1);
    p.getObject(callback1);
    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback2);
    p.getObject(callback2);

    assertEquals('getCount for allocated, Should be 3', getCount1, 3);
    assertEquals('getCount for unallocated, Should be 0', getCount2, 0);
  },

  testExceedMax2() {
    const p = new PriorityPool(0, 1);

    let getCount1 = 0;
    const callback1 = (obj) => {
      assertNotNull(obj);
      getCount1++;
    };

    let getCount2 = 0;
    const callback2 = (obj) => {
      getCount2++;
    };

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback2);
    p.getObject(callback2);
    p.getObject(callback2);
    p.getObject(callback2);

    assertEquals('getCount for allocated, Should be 1', getCount1, 1);
    assertEquals('getCount for unallocated, Should be 0', getCount2, 0);
  },

  testExceedMax3() {
    const p = new PriorityPool(0, 2);

    let obj1 = null;
    const callback1 = (obj) => {
      obj1 = obj;
    };

    let obj2 = null;
    const callback2 = (obj) => {
      obj2 = obj;
    };

    let obj3 = null;
    const callback3 = (obj) => {
      obj3 = obj;
    };

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);

    assertNotNull(obj1);
    assertNotNull(obj2);
    assertNull(obj3);
  },

  testExceedMax4() {
    const p = new PriorityPool();  // default: 10
    const objs = [];

    let getCount1 = 0;
    const callback1 = (obj) => {
      assertNotNull(obj);
      getCount1++;
    };

    let getCount2 = 0;
    const callback2 = (obj) => {
      getCount2++;
    };

    for (let i = 0; i < 12; i++) {
      p.getObject(i < 10 ? callback1 : callback2);
    }

    assertEquals('getCount for allocated, Should be 10', getCount1, 10);
    assertEquals('getCount for unallocated, Should be 0', getCount2, 0);
  },

  testReleaseAndGet1() {
    const p = new PriorityPool(0, 10);

    let o = null;
    const callback = (obj) => {
      o = obj;
    };

    p.getObject(callback);
    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o));
    assertEquals(1, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(1, p.getFreeCount());
  },

  testReleaseAndGet2() {
    const p = new NoObjectReusePriorityPool(0, 10);

    let o = null;
    const callback = (obj) => {
      o = obj;
    };

    p.getObject(callback);
    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o));
    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
  },

  testReleaseAndGet3() {
    const p = new PriorityPool(0, 10);
    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    const o4 = {};

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);

    assertEquals(3, p.getCount());
    assertEquals(3, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o1));
    assertTrue('Result should be true', p.releaseObject(o2));
    assertFalse('Result should be false', p.releaseObject(o4));
    assertEquals(3, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(2, p.getFreeCount());
  },

  testReleaseAndGet4() {
    const p = new NoObjectReusePriorityPool(0, 10);

    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    const o4 = {};

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);
    assertEquals(3, p.getCount());
    assertEquals(3, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o1));
    assertTrue('Result should be true', p.releaseObject(o2));
    assertFalse('Result should be false', p.releaseObject(o4));
    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
  },

  testIsInPool1() {
    const p = new PriorityPool();
    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    const o4 = {};
    const o5 = {};

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);
    const o6 = o1;

    assertTrue(p.contains(o1));
    assertTrue(p.contains(o2));
    assertTrue(p.contains(o3));
    assertFalse(p.contains(o4));
    assertFalse(p.contains(o5));
    assertTrue(p.contains(o6));
  },

  testSetMin1() {
    const p = new PriorityPool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    p.setMinimumCount(10);

    assertEquals(10, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(10, p.getFreeCount());
  },

  testSetMin2() {
    const p = new PriorityPool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };
    p.getObject(callback1);

    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    p.setMinimumCount(10);

    assertEquals(10, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(9, p.getFreeCount());
  },

  testSetMax1() {
    const p = new PriorityPool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    let o4 = null;
    const callback4 = (obj) => {
      o4 = obj;
    };

    let o5 = null;
    const callback5 = (obj) => {
      o5 = obj;
    };

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);
    p.getObject(callback4);
    p.getObject(callback5);

    assertEquals(5, p.getCount());
    assertEquals(5, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    assertTrue('Result should be true', p.releaseObject(o5));

    assertEquals(5, p.getCount());
    assertEquals(4, p.getInUseCount());
    assertEquals(1, p.getFreeCount());

    p.setMaximumCount(4);

    assertEquals(4, p.getCount());
    assertEquals(4, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
  },

  testInvalidMinMax1() {
    const p = new PriorityPool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    assertThrows(() => {
      p.setMinimumCount(11);
    });
  },

  testInvalidMinMax2() {
    const p = new PriorityPool(5, 10);

    assertEquals(5, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(5, p.getFreeCount());

    assertThrows(() => {
      p.setMaximumCount(4);
    });
  },

  testInvalidMinMax3() {
    assertThrows(() => {
      new PriorityPool(10, 1);
    });
  },

  testQueue1() {
    const p = new PriorityPool(0, 2);

    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    p.getObject(callback1);
    p.getObject(callback2);
    p.getObject(callback3);

    assertNotNull(o1);
    assertNotNull(o2);
    assertNull(o3);

    p.releaseObject(o1);
    assertNotNull(o3);
  },

  testPriority1() {
    const p = new PriorityPool(0, 2);

    let o1 = null;
    const callback1 = (obj) => {
      o1 = obj;
    };

    let o2 = null;
    const callback2 = (obj) => {
      o2 = obj;
    };

    let o3 = null;
    const callback3 = (obj) => {
      o3 = obj;
    };

    let o4 = null;
    const callback4 = (obj) => {
      o4 = obj;
    };

    let o5 = null;
    const callback5 = (obj) => {
      o5 = obj;
    };

    let o6 = null;
    const callback6 = (obj) => {
      o6 = obj;
    };

    p.getObject(callback1);  // Initially seeded requests.
    p.getObject(callback2);

    p.getObject(callback3, 101);  // Lowest priority.
    p.getObject(callback4);       // Second lowest priority (default is 100).
    p.getObject(callback5, 99);   // Second highest priority.
    p.getObject(callback6, 0);    // Highest priority.

    assertNotNull(o1);
    assertNotNull(o2);
    assertNull(o3);
    assertNull(o4);
    assertNull(o5);
    assertNull(o6);

    p.releaseObject(o1);  // Release the first initially seeded request (o1).
    assertNotNull(o6);  // Make sure the highest priority request (o6) started.
    assertNull(o3);
    assertNull(o4);
    assertNull(o5);

    p.releaseObject(o2);  // Release the second, initially seeded request (o2).
    assertNotNull(o5);    // The second highest priority request starts (o5).
    assertNull(o3);
    assertNull(o4);

    p.releaseObject(o6);
    assertNotNull(o4);
    assertNull(o3);
  },

  testRateLimiting() {
    const clock = new MockClock();
    clock.install();

    const p = new PriorityPool(0, 4);
    p.setDelay(100);

    let getCount = 0;
    const callback = (obj) => {
      assertNotNull(obj);
      getCount++;
    };

    p.getObject(callback);
    assertEquals(1, getCount);

    p.getObject(callback);
    assertEquals(1, getCount);

    clock.tick(100);
    assertEquals(2, getCount);

    p.getObject(callback);
    p.getObject(callback);
    assertEquals(2, getCount);

    clock.tick(100);
    assertEquals(3, getCount);

    clock.tick(100);
    assertEquals(4, getCount);

    p.getObject(callback);
    assertEquals(4, getCount);

    clock.tick(100);
    assertEquals(4, getCount);

    dispose(clock);
  },

  testRateLimitingWithChangingDelay() {
    const clock = new MockClock();
    clock.install();

    const p = new PriorityPool(0, 3);
    p.setDelay(100);

    let getCount = 0;
    const callback = (obj) => {
      assertNotNull(obj);
      getCount++;
    };

    p.getObject(callback);
    assertEquals(1, getCount);

    p.getObject(callback);
    assertEquals(1, getCount);

    clock.tick(50);
    assertEquals(1, getCount);

    p.setDelay(50);
    assertEquals(2, getCount);

    p.getObject(callback);
    assertEquals(2, getCount);

    clock.tick(20);
    assertEquals(2, getCount);

    p.setDelay(40);
    assertEquals(2, getCount);

    clock.tick(20);
    assertEquals(3, getCount);

    dispose(clock);
  },
});

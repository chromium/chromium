/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.PoolTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const Pool = goog.require('goog.structs.Pool');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

// Implementation of the Pool class with isObjectDead() always returning TRUE,
// so that the Pool will not reuse any objects.
class NoObjectReusePool extends Pool {
  constructor(min = undefined, max = undefined) {
    super(min, max);
  }

  objectCanBeReused(obj) {
    return false;
  }
}

testSuite({
  testExceedMax1() {
    const p = new Pool(0, 3);
    const obj1 = p.getObject();
    const obj2 = p.getObject();
    const obj3 = p.getObject();
    const obj4 = p.getObject();
    const obj5 = p.getObject();

    assertNotUndefined(obj1);
    assertNotUndefined(obj2);
    assertNotUndefined(obj3);
    assertUndefined(obj4);
    assertUndefined(obj5);
  },

  testExceedMax2() {
    const p = new Pool(0, 1);
    const obj1 = p.getObject();
    const obj2 = p.getObject();
    const obj3 = p.getObject();
    const obj4 = p.getObject();
    const obj5 = p.getObject();

    assertNotUndefined(obj1);
    assertUndefined(obj2);
    assertUndefined(obj3);
    assertUndefined(obj4);
    assertUndefined(obj5);
  },

  testExceedMax3() {
    const p = new Pool();  // default: 10
    const objs = [];

    for (let i = 0; i < 12; i++) {
      objs[i] = p.getObject();
    }

    for (let i = 0; i < 10; i++) {
      assertNotNull('First 10 should be not null', objs[i]);
    }

    assertUndefined(objs[10]);
    assertUndefined(objs[11]);
  },

  testReleaseAndGet1() {
    const p = new Pool(0, 10);
    const o = p.getObject();
    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o));
    assertEquals(1, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(1, p.getFreeCount());
  },

  testReleaseAndGet2() {
    const p = new NoObjectReusePool(0, 10);
    const o = p.getObject();
    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
    assertTrue('Result should be true', p.releaseObject(o));
    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());
  },

  testReleaseAndGet3() {
    const p = new Pool(0, 10);
    const o1 = p.getObject();
    const o2 = p.getObject();
    const o3 = p.getObject();
    const o4 = {};
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
    const p = new NoObjectReusePool(0, 10);
    const o1 = p.getObject();
    const o2 = p.getObject();
    const o3 = p.getObject();
    const o4 = {};
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
    const p = new Pool();
    const o1 = p.getObject();
    const o2 = p.getObject();
    const o3 = p.getObject();
    const o4 = {};
    const o5 = {};
    const o6 = o1;

    assertTrue(p.contains(o1));
    assertTrue(p.contains(o2));
    assertTrue(p.contains(o3));
    assertFalse(p.contains(o4));
    assertFalse(p.contains(o5));
    assertTrue(p.contains(o6));
  },

  testSetMin1() {
    const p = new Pool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    p.setMinimumCount(10);

    assertEquals(10, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(10, p.getFreeCount());
  },

  testSetMin2() {
    const p = new Pool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    const o1 = p.getObject();

    assertEquals(1, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    p.setMinimumCount(10);

    assertEquals(10, p.getCount());
    assertEquals(1, p.getInUseCount());
    assertEquals(9, p.getFreeCount());
  },

  testSetMax1() {
    const p = new Pool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    const o1 = p.getObject();
    const o2 = p.getObject();
    const o3 = p.getObject();
    const o4 = p.getObject();
    const o5 = p.getObject();

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
    const p = new Pool(0, 10);

    assertEquals(0, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(0, p.getFreeCount());

    assertThrows(() => {
      p.setMinimumCount(11);
    });
  },

  testInvalidMinMax2() {
    const p = new Pool(5, 10);

    assertEquals(5, p.getCount());
    assertEquals(0, p.getInUseCount());
    assertEquals(5, p.getFreeCount());

    assertThrows(() => {
      p.setMaximumCount(4);
    });
  },

  testInvalidMinMax3() {
    assertThrows(() => {
      new Pool(10, 1);
    });
  },

  testRateLimiting() {
    const clock = new MockClock();
    clock.install();

    const p = new Pool(0, 3);
    p.setDelay(100);

    assertNotUndefined(p.getObject());
    assertUndefined(p.getObject());

    clock.tick(100);
    assertNotUndefined(p.getObject());
    assertUndefined(p.getObject());

    clock.tick(100);
    assertNotUndefined(p.getObject());
    assertUndefined(p.getObject());

    clock.tick(100);
    assertUndefined(p.getObject());

    dispose(clock);
  },
});

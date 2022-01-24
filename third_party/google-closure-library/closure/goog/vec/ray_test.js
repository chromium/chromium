/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.RayTest');
goog.setTestOnly();

const Ray = goog.require('goog.vec.Ray');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testConstructor() {
    let new_ray = new Ray();
    assertElementsEquals([0, 0, 0], new_ray.origin);
    assertElementsEquals([0, 0, 0], new_ray.dir);

    new_ray = new Ray([1, 2, 3], [4, 5, 6]);
    assertElementsEquals([1, 2, 3], new_ray.origin);
    assertElementsEquals([4, 5, 6], new_ray.dir);
  },

  testSet() {
    const new_ray = new Ray();
    new_ray.set([2, 3, 4], [5, 6, 7]);
    assertElementsEquals([2, 3, 4], new_ray.origin);
    assertElementsEquals([5, 6, 7], new_ray.dir);
  },

  testSetOrigin() {
    const new_ray = new Ray();
    new_ray.setOrigin([1, 2, 3]);
    assertElementsEquals([1, 2, 3], new_ray.origin);
    assertElementsEquals([0, 0, 0], new_ray.dir);
  },

  testSetDir() {
    const new_ray = new Ray();
    new_ray.setDir([2, 3, 4]);
    assertElementsEquals([0, 0, 0], new_ray.origin);
    assertElementsEquals([2, 3, 4], new_ray.dir);
  },

  testEquals() {
    const r0 = new Ray([1, 2, 3], [4, 5, 6]);
    const r1 = new Ray([5, 2, 3], [4, 5, 6]);
    assertFalse(r0.equals(r1));
    assertFalse(r0.equals(null));
    assertTrue(r1.equals(r1));
    r1.setOrigin(r0.origin);
    assertTrue(r1.equals(r0));
    assertTrue(r0.equals(r1));
  },
});

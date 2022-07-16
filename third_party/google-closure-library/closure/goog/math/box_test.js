/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.BoxTest');
goog.setTestOnly();

const Box = goog.require('goog.math.Box');
const Coordinate = goog.require('goog.math.Coordinate');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBoxEquals() {
    const a = new Box(1, 2, 3, 4);
    const b = new Box(1, 2, 3, 4);
    assertTrue(Box.equals(a, a));
    assertTrue(Box.equals(a, b));
    assertTrue(Box.equals(b, a));

    assertFalse('Box should not equal null.', Box.equals(a, null));
    assertFalse('Box should not equal null.', Box.equals(null, a));

    assertFalse(Box.equals(a, new Box(4, 2, 3, 4)));
    assertFalse(Box.equals(a, new Box(1, 4, 3, 4)));
    assertFalse(Box.equals(a, new Box(1, 2, 4, 4)));
    assertFalse(Box.equals(a, new Box(1, 2, 3, 1)));

    assertTrue('Null boxes should be equal.', Box.equals(null, null));
  },

  testBoxClone() {
    const b = new Box(0, 0, 0, 0);
    assertTrue(Box.equals(b, b.clone()));

    b.left = 0;
    b.top = 1;
    b.right = 2;
    b.bottom = 3;
    assertTrue(Box.equals(b, b.clone()));
  },

  testBoxRelativePositionX() {
    const box = new Box(50, 100, 100, 50);

    assertEquals(0, Box.relativePositionX(box, new Coordinate(75, 0)));
    assertEquals(0, Box.relativePositionX(box, new Coordinate(75, 75)));
    assertEquals(0, Box.relativePositionX(box, new Coordinate(75, 105)));
    assertEquals(-5, Box.relativePositionX(box, new Coordinate(45, 75)));
    assertEquals(5, Box.relativePositionX(box, new Coordinate(105, 75)));
  },

  testBoxRelativePositionY() {
    const box = new Box(50, 100, 100, 50);

    assertEquals(0, Box.relativePositionY(box, new Coordinate(0, 75)));
    assertEquals(0, Box.relativePositionY(box, new Coordinate(75, 75)));
    assertEquals(0, Box.relativePositionY(box, new Coordinate(105, 75)));
    assertEquals(-5, Box.relativePositionY(box, new Coordinate(75, 45)));
    assertEquals(5, Box.relativePositionY(box, new Coordinate(75, 105)));
  },

  testBoxDistance() {
    const box = new Box(50, 100, 100, 50);

    assertEquals(0, Box.distance(box, new Coordinate(75, 75)));
    assertEquals(25, Box.distance(box, new Coordinate(75, 25)));
    assertEquals(10, Box.distance(box, new Coordinate(40, 80)));
    assertEquals(5, Box.distance(box, new Coordinate(46, 47)));
    assertEquals(10, Box.distance(box, new Coordinate(106, 108)));
  },

  testBoxContains() {
    const box = new Box(50, 100, 100, 50);

    assertTrue(Box.contains(box, new Coordinate(75, 75)));
    assertTrue(Box.contains(box, new Coordinate(50, 100)));
    assertTrue(Box.contains(box, new Coordinate(100, 99)));
    assertFalse(Box.contains(box, new Coordinate(100, 101)));
    assertFalse(Box.contains(box, new Coordinate(49, 50)));
    assertFalse(Box.contains(box, new Coordinate(25, 25)));
  },

  testBoxContainsBox() {
    const box = new Box(50, 100, 100, 50);

    function assertContains(boxB) {
      assertTrue(`${box} expected to contain ${boxB}`, Box.contains(box, boxB));
    }

    function assertNotContains(boxB) {
      assertFalse(
          `${box} expected to not contain ${boxB}`, Box.contains(box, boxB));
    }

    assertContains(new Box(60, 90, 90, 60));
    assertNotContains(new Box(1, 3, 4, 2));
    assertNotContains(new Box(30, 90, 60, 60));
    assertNotContains(new Box(60, 110, 60, 60));
    assertNotContains(new Box(60, 90, 110, 60));
    assertNotContains(new Box(60, 90, 60, 40));
  },

  testBoxesIntersect() {
    const box = new Box(50, 100, 100, 50);

    function assertIntersects(boxB) {
      assertTrue(
          `${box} expected to intersect ${boxB}`, Box.intersects(box, boxB));
    }
    function assertNotIntersects(boxB) {
      assertFalse(
          `${box} expected to not intersect ${boxB}`,
          Box.intersects(box, boxB));
    }

    assertIntersects(box);
    assertIntersects(new Box(20, 80, 80, 20));
    assertIntersects(new Box(50, 80, 100, 20));
    assertIntersects(new Box(80, 80, 120, 20));
    assertIntersects(new Box(20, 100, 80, 50));
    assertIntersects(new Box(80, 100, 120, 50));
    assertIntersects(new Box(20, 120, 80, 80));
    assertIntersects(new Box(50, 120, 100, 80));
    assertIntersects(new Box(80, 120, 120, 80));
    assertIntersects(new Box(20, 120, 120, 20));
    assertIntersects(new Box(70, 80, 80, 70));
    assertNotIntersects(new Box(10, 30, 30, 10));
    assertNotIntersects(new Box(10, 70, 30, 30));
    assertNotIntersects(new Box(10, 100, 30, 50));
    assertNotIntersects(new Box(10, 120, 30, 80));
    assertNotIntersects(new Box(10, 140, 30, 120));
    assertNotIntersects(new Box(30, 30, 70, 10));
    assertNotIntersects(new Box(30, 140, 70, 120));
    assertNotIntersects(new Box(50, 30, 100, 10));
    assertNotIntersects(new Box(50, 140, 100, 120));
    assertNotIntersects(new Box(80, 30, 120, 10));
    assertNotIntersects(new Box(80, 140, 120, 120));
    assertNotIntersects(new Box(120, 30, 140, 10));
    assertNotIntersects(new Box(120, 70, 140, 30));
    assertNotIntersects(new Box(120, 100, 140, 50));
    assertNotIntersects(new Box(120, 120, 140, 80));
    assertNotIntersects(new Box(120, 140, 140, 120));
  },

  testBoxesIntersectWithPadding() {
    const box = new Box(50, 100, 100, 50);

    function assertIntersects(boxB, padding) {
      assertTrue(
          `${box} expected to intersect ${boxB} with padding ${padding}`,
          Box.intersectsWithPadding(box, boxB, padding));
    }
    function assertNotIntersects(boxB, padding) {
      assertFalse(
          `${box} expected to not intersect ${boxB} with padding ${padding}`,
          Box.intersectsWithPadding(box, boxB, padding));
    }

    assertIntersects(box, 10);
    assertIntersects(new Box(20, 80, 80, 20), 10);
    assertIntersects(new Box(50, 80, 100, 20), 10);
    assertIntersects(new Box(80, 80, 120, 20), 10);
    assertIntersects(new Box(20, 100, 80, 50), 10);
    assertIntersects(new Box(80, 100, 120, 50), 10);
    assertIntersects(new Box(20, 120, 80, 80), 10);
    assertIntersects(new Box(50, 120, 100, 80), 10);
    assertIntersects(new Box(80, 120, 120, 80), 10);
    assertIntersects(new Box(20, 120, 120, 20), 10);
    assertIntersects(new Box(70, 80, 80, 70), 10);
    assertIntersects(new Box(10, 30, 30, 10), 20);
    assertIntersects(new Box(10, 70, 30, 30), 20);
    assertIntersects(new Box(10, 100, 30, 50), 20);
    assertIntersects(new Box(10, 120, 30, 80), 20);
    assertIntersects(new Box(10, 140, 30, 120), 20);
    assertIntersects(new Box(30, 30, 70, 10), 20);
    assertIntersects(new Box(30, 140, 70, 120), 20);
    assertIntersects(new Box(50, 30, 100, 10), 20);
    assertIntersects(new Box(50, 140, 100, 120), 20);
    assertIntersects(new Box(80, 30, 120, 10), 20);
    assertIntersects(new Box(80, 140, 120, 120), 20);
    assertIntersects(new Box(120, 30, 140, 10), 20);
    assertIntersects(new Box(120, 70, 140, 30), 20);
    assertIntersects(new Box(120, 100, 140, 50), 20);
    assertIntersects(new Box(120, 120, 140, 80), 20);
    assertIntersects(new Box(120, 140, 140, 120), 20);
    assertNotIntersects(new Box(10, 30, 30, 10), 10);
    assertNotIntersects(new Box(10, 70, 30, 30), 10);
    assertNotIntersects(new Box(10, 100, 30, 50), 10);
    assertNotIntersects(new Box(10, 120, 30, 80), 10);
    assertNotIntersects(new Box(10, 140, 30, 120), 10);
    assertNotIntersects(new Box(30, 30, 70, 10), 10);
    assertNotIntersects(new Box(30, 140, 70, 120), 10);
    assertNotIntersects(new Box(50, 30, 100, 10), 10);
    assertNotIntersects(new Box(50, 140, 100, 120), 10);
    assertNotIntersects(new Box(80, 30, 120, 10), 10);
    assertNotIntersects(new Box(80, 140, 120, 120), 10);
    assertNotIntersects(new Box(120, 30, 140, 10), 10);
    assertNotIntersects(new Box(120, 70, 140, 30), 10);
    assertNotIntersects(new Box(120, 100, 140, 50), 10);
    assertNotIntersects(new Box(120, 120, 140, 80), 10);
    assertNotIntersects(new Box(120, 140, 140, 120), 10);
  },

  testExpandToInclude() {
    const box = new Box(10, 50, 50, 10);
    box.expandToInclude(new Box(60, 70, 70, 60));
    assertEquals(10, box.left);
    assertEquals(10, box.top);
    assertEquals(70, box.right);
    assertEquals(70, box.bottom);
    box.expandToInclude(new Box(30, 40, 40, 30));
    assertEquals(10, box.left);
    assertEquals(10, box.top);
    assertEquals(70, box.right);
    assertEquals(70, box.bottom);
    box.expandToInclude(new Box(0, 100, 100, 0));
    assertEquals(0, box.left);
    assertEquals(0, box.top);
    assertEquals(100, box.right);
    assertEquals(100, box.bottom);
  },

  testExpandToIncludeCoordinate() {
    const box = new Box(10, 50, 50, 10);
    box.expandToIncludeCoordinate(new Coordinate(0, 0));
    assertEquals(0, box.left);
    assertEquals(0, box.top);
    assertEquals(50, box.right);
    assertEquals(50, box.bottom);
    box.expandToIncludeCoordinate(new Coordinate(100, 0));
    assertEquals(0, box.left);
    assertEquals(0, box.top);
    assertEquals(100, box.right);
    assertEquals(50, box.bottom);
    box.expandToIncludeCoordinate(new Coordinate(0, 100));
    assertEquals(0, box.left);
    assertEquals(0, box.top);
    assertEquals(100, box.right);
    assertEquals(100, box.bottom);
  },

  testGetWidth() {
    const box = new Box(10, 50, 30, 25);
    assertEquals(25, box.getWidth());
  },

  testGetHeight() {
    const box = new Box(10, 50, 30, 25);
    assertEquals(20, box.getHeight());
  },

  testBoundingBox() {
    assertObjectEquals(
        new Box(1, 10, 11, 0),
        Box.boundingBox(
            new Coordinate(5, 5), new Coordinate(5, 11), new Coordinate(0, 5),
            new Coordinate(5, 1), new Coordinate(10, 5)));
  },

  testBoxCeil() {
    const box = new Box(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', box, box.ceil());
    assertObjectEquals(new Box(12, 27, 18, 10), box);
  },

  testBoxFloor() {
    const box = new Box(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', box, box.floor());
    assertObjectEquals(new Box(11, 26, 17, 9), box);
  },

  testBoxRound() {
    const box = new Box(11.4, 26.6, 17.8, 9.2);
    assertEquals(
        'The function should return the target instance', box, box.round());
    assertObjectEquals(new Box(11, 27, 18, 9), box);
  },

  testBoxTranslateCoordinate() {
    const box = new Box(10, 30, 20, 5);
    const c = new Coordinate(10, 5);
    assertEquals(
        'The function should return the target instance', box,
        box.translate(c));
    assertObjectEquals(new Box(15, 40, 25, 15), box);
  },

  testBoxTranslateXY() {
    const box = new Box(10, 30, 20, 5);
    assertEquals(
        'The function should return the target instance', box,
        box.translate(5, 2));
    assertObjectEquals(new Box(12, 35, 22, 10), box);
  },

  testBoxTranslateX() {
    const box = new Box(10, 30, 20, 5);
    assertEquals(
        'The function should return the target instance', box,
        box.translate(3));
    assertObjectEquals(new Box(10, 33, 20, 8), box);
  },

  testBoxScaleXY() {
    const box = new Box(10, 20, 30, 5);
    assertEquals(
        'The function should return the target instance', box, box.scale(2, 3));
    assertObjectEquals(new Box(30, 40, 90, 10), box);
  },

  testBoxScaleFactor() {
    const box = new Box(10, 20, 30, 5);
    assertEquals(
        'The function should return the target instance', box, box.scale(2));
    assertObjectEquals(new Box(20, 40, 60, 10), box);
  },
});

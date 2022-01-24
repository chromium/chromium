/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.SizeTest');
goog.setTestOnly();

const Size = goog.require('goog.math.Size');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSize1() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const s = new Size(undefined, undefined);
    assertUndefined(s.width);
    assertUndefined(s.height);
    assertEquals('(undefined x undefined)', s.toString());
  },

  testSize3() {
    const s = new Size(10, 20);
    assertEquals(10, s.width);
    assertEquals(20, s.height);
    assertEquals('(10 x 20)', s.toString());
  },

  testSize4() {
    const s = new Size(10.5, 20.897);
    assertEquals(10.5, s.width);
    assertEquals(20.897, s.height);
    assertEquals('(10.5 x 20.897)', s.toString());
  },

  testSizeClone() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const s = new Size(undefined, undefined);
    assertEquals(s.toString(), s.clone().toString());
    s.width = 4;
    s.height = 5;
    assertEquals(s.toString(), s.clone().toString());
  },

  testSizeEquals() {
    const a = new Size(4, 5);

    assertTrue(Size.equals(a, a));
    assertFalse(Size.equals(a, null));
    assertFalse(Size.equals(null, a));

    const b = new Size(4, 5);
    const c = new Size(4, 6);
    assertTrue(Size.equals(a, b));
    assertFalse(Size.equals(a, c));
  },

  testSizeArea() {
    const s = new Size(4, 5);
    assertEquals(20, s.area());
  },

  testSizePerimeter() {
    const s = new Size(4, 5);
    assertEquals(18, s.perimeter());
  },

  testSizeAspectRatio() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const s = new Size(undefined, undefined);
    assertNaN(s.aspectRatio());

    s.width = 4;
    s.height = 0;
    assertEquals(Infinity, s.aspectRatio());

    s.height = 5;
    assertEquals(0.8, s.aspectRatio());
  },

  testSizeFitsInside() {
    const target = new Size(10, 10);

    const a = new Size(5, 8);
    const b = new Size(5, 12);
    const c = new Size(19, 7);

    assertTrue(a.fitsInside(target));
    assertFalse(b.fitsInside(target));
    assertFalse(c.fitsInside(target));
  },

  testSizeScaleToCover() {
    const target = new Size(512, 640);

    const a = new Size(1000, 1600);
    const b = new Size(1600, 1000);
    const c = new Size(500, 800);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const d = new Size(undefined, undefined);

    assertEquals('(512 x 819.2)', a.scaleToCover(target).toString());
    assertEquals('(1024 x 640)', b.scaleToCover(target).toString());
    assertEquals('(512 x 819.2)', c.scaleToCover(target).toString());
    assertEquals('(512 x 640)', target.scaleToCover(target).toString());

    assertEquals('(NaN x NaN)', d.scaleToCover(target).toString());
    assertEquals('(NaN x NaN)', a.scaleToCover(d).toString());
  },

  testSizeScaleToFit() {
    const target = new Size(512, 640);

    const a = new Size(1600, 1200);
    const b = new Size(1200, 1600);
    const c = new Size(400, 300);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const d = new Size(undefined, undefined);

    assertEquals('(512 x 384)', a.scaleToFit(target).toString());
    assertEquals('(480 x 640)', b.scaleToFit(target).toString());
    assertEquals('(512 x 384)', c.scaleToFit(target).toString());
    assertEquals('(512 x 640)', target.scaleToFit(target).toString());

    assertEquals('(NaN x NaN)', d.scaleToFit(target).toString());
    assertEquals('(NaN x NaN)', a.scaleToFit(d).toString());
  },

  testSizeIsEmpty() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const s = new Size(undefined, undefined);
    assertTrue(s.isEmpty());
    s.width = 0;
    s.height = 5;
    assertTrue(s.isEmpty());
    s.width = 4;
    assertFalse(s.isEmpty());
  },

  testSizeScaleFactor() {
    const s = new Size(4, 5);
    assertEquals('(8 x 10)', s.scale(2).toString());
    assertEquals('(0.8 x 1)', s.scale(0.1).toString());
  },

  testSizeCeil() {
    const s = new Size(2.3, 4.7);
    assertEquals('(3 x 5)', s.ceil().toString());
  },

  testSizeFloor() {
    const s = new Size(2.3, 4.7);
    assertEquals('(2 x 4)', s.floor().toString());
  },

  testSizeRound() {
    const s = new Size(2.3, 4.7);
    assertEquals('(2 x 5)', s.round().toString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSizeGetLongest() {
    const s = new Size(3, 4);
    assertEquals(4, s.getLongest());

    s.height = 3;
    assertEquals(3, s.getLongest());

    s.height = 2;
    assertEquals(3, s.getLongest());

    assertNaN(new Size(undefined, undefined).getLongest());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSizeGetShortest() {
    const s = new Size(3, 4);
    assertEquals(3, s.getShortest());

    s.height = 3;
    assertEquals(3, s.getShortest());

    s.height = 2;
    assertEquals(2, s.getShortest());

    assertNaN(new Size(undefined, undefined).getShortest());
  },

  testSizeScaleXY() {
    const s = new Size(5, 10);
    assertEquals('(20 x 30)', s.scale(4, 3).toString());
  },
});

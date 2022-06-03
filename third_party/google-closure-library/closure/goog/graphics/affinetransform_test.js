/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.AffineTransformTest');
goog.setTestOnly();

const AffineTransform = goog.require('goog.graphics.AffineTransform');
const googArray = goog.require('goog.array');
const googMath = goog.require('goog.math');
const graphics = goog.require('goog.graphics');
const testSuite = goog.require('goog.testing.testSuite');

function assertEqualsMethod(tx1, tx2, expected) {
  assertEquals(expected, tx1.equals(tx2));
  assertEquals(expected, tx2.equals(tx1));
  assertEquals(true, tx1.equals(tx1));
  assertEquals(true, tx2.equals(tx2));
}
testSuite({
  testGetTranslateInstance() {
    const tx = AffineTransform.getTranslateInstance(2, 4);
    assertEquals(1, tx.getScaleX());
    assertEquals(0, tx.getShearY());
    assertEquals(0, tx.getShearX());
    assertEquals(1, tx.getScaleY());
    assertEquals(2, tx.getTranslateX());
    assertEquals(4, tx.getTranslateY());
  },

  testGetScaleInstance() {
    const tx = AffineTransform.getScaleInstance(2, 4);
    assertEquals(2, tx.getScaleX());
    assertEquals(0, tx.getShearY());
    assertEquals(0, tx.getShearX());
    assertEquals(4, tx.getScaleY());
    assertEquals(0, tx.getTranslateX());
    assertEquals(0, tx.getTranslateY());
  },

  testGetRotateInstance() {
    const tx = AffineTransform.getRotateInstance(Math.PI / 2, 1, 2);
    assertRoughlyEquals(0, tx.getScaleX(), 1e-9);
    assertRoughlyEquals(1, tx.getShearY(), 1e-9);
    assertRoughlyEquals(-1, tx.getShearX(), 1e-9);
    assertRoughlyEquals(0, tx.getScaleY(), 1e-9);
    assertRoughlyEquals(3, tx.getTranslateX(), 1e-9);
    assertRoughlyEquals(1, tx.getTranslateY(), 1e-9);
  },

  testGetShearInstance() {
    const tx = AffineTransform.getShearInstance(2, 4);
    assertEquals(1, tx.getScaleX());
    assertEquals(4, tx.getShearY());
    assertEquals(2, tx.getShearX());
    assertEquals(1, tx.getScaleY());
    assertEquals(0, tx.getTranslateX());
    assertEquals(0, tx.getTranslateY());
  },

  testConstructor() {
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   new AffineTransform([0, 0]);
                 });
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   new AffineTransform({});
                 });
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   new AffineTransform(0, 0, 0, 'a', 0, 0);
                 });

    let tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    assertEquals(1, tx.getScaleX());
    assertEquals(2, tx.getShearY());
    assertEquals(3, tx.getShearX());
    assertEquals(4, tx.getScaleY());
    assertEquals(5, tx.getTranslateX());
    assertEquals(6, tx.getTranslateY());

    tx = new AffineTransform();
    assert(tx.isIdentity());
  },

  testIsIdentity() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    assertFalse(tx.isIdentity());
    tx.setTransform(1, 0, 0, 1, 0, 0);
    assert(tx.isIdentity());
  },

  testClone() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    const copy = tx.clone();
    assertEquals(copy.getScaleX(), tx.getScaleX());
    assertEquals(copy.getShearY(), tx.getShearY());
    assertEquals(copy.getShearX(), tx.getShearX());
    assertEquals(copy.getScaleY(), tx.getScaleY());
    assertEquals(copy.getTranslateX(), tx.getTranslateX());
    assertEquals(copy.getTranslateY(), tx.getTranslateY());
  },

  testSetTransform() {
    const tx = new AffineTransform();
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   tx.setTransform(1, 2, 3, 4, 6);
                 });
    assertThrows(/**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
                 () => {
                   tx.setTransform('a', 2, 3, 4, 5, 6);
                 });

    tx.setTransform(1, 2, 3, 4, 5, 6);
    assertEquals(1, tx.getScaleX());
    assertEquals(2, tx.getShearY());
    assertEquals(3, tx.getShearX());
    assertEquals(4, tx.getScaleY());
    assertEquals(5, tx.getTranslateX());
    assertEquals(6, tx.getTranslateY());
  },

  testScale() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.scale(2, 3);
    assertEquals(2, tx.getScaleX());
    assertEquals(4, tx.getShearY());
    assertEquals(9, tx.getShearX());
    assertEquals(12, tx.getScaleY());
    assertEquals(5, tx.getTranslateX());
    assertEquals(6, tx.getTranslateY());
  },

  testPreScale() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.preScale(2, 3);
    assertEquals(2, tx.getScaleX());
    assertEquals(6, tx.getShearY());
    assertEquals(6, tx.getShearX());
    assertEquals(12, tx.getScaleY());
    assertEquals(10, tx.getTranslateX());
    assertEquals(18, tx.getTranslateY());
  },

  testTranslate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.translate(2, 3);
    assertEquals(1, tx.getScaleX());
    assertEquals(2, tx.getShearY());
    assertEquals(3, tx.getShearX());
    assertEquals(4, tx.getScaleY());
    assertEquals(16, tx.getTranslateX());
    assertEquals(22, tx.getTranslateY());
  },

  testPreTranslate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.preTranslate(2, 3);
    assertEquals(1, tx.getScaleX());
    assertEquals(2, tx.getShearY());
    assertEquals(3, tx.getShearX());
    assertEquals(4, tx.getScaleY());
    assertEquals(7, tx.getTranslateX());
    assertEquals(9, tx.getTranslateY());
  },

  testRotate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.rotate(Math.PI / 2, 1, 1);
    assertRoughlyEquals(3, tx.getScaleX(), 1e-9);
    assertRoughlyEquals(4, tx.getShearY(), 1e-9);
    assertRoughlyEquals(-1, tx.getShearX(), 1e-9);
    assertRoughlyEquals(-2, tx.getScaleY(), 1e-9);
    assertRoughlyEquals(7, tx.getTranslateX(), 1e-9);
    assertRoughlyEquals(10, tx.getTranslateY(), 1e-9);
  },

  testPreRotate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.preRotate(Math.PI / 2, 1, 1);
    assertRoughlyEquals(-2, tx.getScaleX(), 1e-9);
    assertRoughlyEquals(1, tx.getShearY(), 1e-9);
    assertRoughlyEquals(-4, tx.getShearX(), 1e-9);
    assertRoughlyEquals(3, tx.getScaleY(), 1e-9);
    assertRoughlyEquals(-4, tx.getTranslateX(), 1e-9);
    assertRoughlyEquals(5, tx.getTranslateY(), 1e-9);
  },

  testShear() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.shear(2, 3);
    assertEquals(10, tx.getScaleX());
    assertEquals(14, tx.getShearY());
    assertEquals(5, tx.getShearX());
    assertEquals(8, tx.getScaleY());
    assertEquals(5, tx.getTranslateX());
    assertEquals(6, tx.getTranslateY());
  },

  testPreShear() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.preShear(2, 3);
    assertEquals(5, tx.getScaleX());
    assertEquals(5, tx.getShearY());
    assertEquals(11, tx.getShearX());
    assertEquals(13, tx.getScaleY());
    assertEquals(17, tx.getTranslateX());
    assertEquals(21, tx.getTranslateY());
  },

  testConcatentate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.concatenate(new AffineTransform(2, 1, 6, 5, 4, 3));
    assertEquals(5, tx.getScaleX());
    assertEquals(8, tx.getShearY());
    assertEquals(21, tx.getShearX());
    assertEquals(32, tx.getScaleY());
    assertEquals(18, tx.getTranslateX());
    assertEquals(26, tx.getTranslateY());
  },

  testPreConcatentate() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    tx.preConcatenate(new AffineTransform(2, 1, 6, 5, 4, 3));
    assertEquals(14, tx.getScaleX());
    assertEquals(11, tx.getShearY());
    assertEquals(30, tx.getShearX());
    assertEquals(23, tx.getScaleY());
    assertEquals(50, tx.getTranslateX());
    assertEquals(38, tx.getTranslateY());
  },

  testAssociativeConcatenate() {
    const x = new AffineTransform(2, 3, 5, 7, 11, 13)
                  .concatenate(new AffineTransform(17, 19, 23, 29, 31, 37));
    const y = new AffineTransform(17, 19, 23, 29, 31, 37)
                  .preConcatenate(new AffineTransform(2, 3, 5, 7, 11, 13));
    assertEquals(x.getScaleX(), y.getScaleX());
    assertEquals(x.getShearY(), y.getShearY());
    assertEquals(x.getShearX(), y.getShearX());
    assertEquals(x.getScaleY(), y.getScaleY());
    assertEquals(x.getTranslateX(), y.getTranslateX());
    assertEquals(x.getTranslateY(), y.getTranslateY());
  },

  testTransform() {
    const srcPts = [0, 0, 1, 0, 1, 1, 0, 1];
    const dstPts = [];
    const tx = AffineTransform.getScaleInstance(2, 3);
    tx.translate(5, 10);
    tx.rotate(Math.PI / 4, 5, 10);
    tx.transform(srcPts, 0, dstPts, 0, 4);
    assert(googArray.equals(
        [
          27.071068, 28.180195, 28.485281, 30.301516, 27.071068, 32.422836,
          25.656855, 30.301516
        ],
        dstPts, googMath.nearlyEquals));
  },

  testGetDeterminant() {
    const tx = AffineTransform.getScaleInstance(2, 3);
    tx.translate(5, 10);
    tx.rotate(Math.PI / 4, 5, 10);
    assertRoughlyEquals(6, tx.getDeterminant(), 0.001);
  },

  testIsInvertible() {
    assertTrue(new AffineTransform(2, 3, 4, 5, 6, 7).isInvertible());
    assertTrue(new AffineTransform(1, 0, 0, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(NaN, 0, 0, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, NaN, 0, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, NaN, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, NaN, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, 1, NaN, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, 1, 0, NaN).isInvertible());
    assertFalse(new AffineTransform(Infinity, 0, 0, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, Infinity, 0, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, Infinity, 1, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, Infinity, 0, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, 1, Infinity, 0).isInvertible());
    assertFalse(new AffineTransform(1, 0, 0, 1, 0, Infinity).isInvertible());
    assertFalse(new AffineTransform(0, 0, 0, 0, 1, 0).isInvertible());
  },

  testCreateInverse() {
    const tx = AffineTransform.getScaleInstance(2, 3);
    tx.translate(5, 10);
    tx.rotate(Math.PI / 4, 5, 10);
    const inverse = tx.createInverse();
    assert(googMath.nearlyEquals(0.353553, inverse.getScaleX()));
    assert(googMath.nearlyEquals(-0.353553, inverse.getShearY()));
    assert(googMath.nearlyEquals(0.235702, inverse.getShearX()));
    assert(googMath.nearlyEquals(0.235702, inverse.getScaleY()));
    assert(googMath.nearlyEquals(-16.213203, inverse.getTranslateX()));
    assert(googMath.nearlyEquals(2.928932, inverse.getTranslateY()));
  },

  testCopyFrom() {
    const from = new AffineTransform(1, 2, 3, 4, 5, 6);
    const to = new AffineTransform();
    to.copyFrom(from);
    assertEquals(from.getScaleX(), to.getScaleX());
    assertEquals(from.getShearY(), to.getShearY());
    assertEquals(from.getShearX(), to.getShearX());
    assertEquals(from.getScaleY(), to.getScaleY());
    assertEquals(from.getTranslateX(), to.getTranslateX());
    assertEquals(from.getTranslateY(), to.getTranslateY());
  },

  testToString() {
    const tx = new AffineTransform(1, 2, 3, 4, 5, 6);
    assertEquals('matrix(1,2,3,4,5,6)', tx.toString());
  },

  testEquals() {
    const tx1 = new AffineTransform(1, 2, 3, 4, 5, 6);
    let tx2 = new AffineTransform(1, 2, 3, 4, 5, 6);
    assertEqualsMethod(tx1, tx2, true);

    tx2 = new AffineTransform(-1, 2, 3, 4, 5, 6);
    assertEqualsMethod(tx1, tx2, false);

    tx2 = new AffineTransform(1, -1, 3, 4, 5, 6);
    assertEqualsMethod(tx1, tx2, false);

    tx2 = new AffineTransform(1, 2, -3, 4, 5, 6);
    assertEqualsMethod(tx1, tx2, false);

    tx2 = new AffineTransform(1, 2, 3, -4, 5, 6);
    assertEqualsMethod(tx1, tx2, false);

    tx2 = new AffineTransform(1, 2, 3, 4, -5, 6);
    assertEqualsMethod(tx1, tx2, false);

    tx2 = new AffineTransform(1, 2, 3, 4, 5, -6);
    assertEqualsMethod(tx1, tx2, false);
  },
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.QuaternionTest');
goog.setTestOnly();

const Mat3 = goog.require('goog.vec.Mat3');
const Mat4 = goog.require('goog.vec.Mat4');
const Quaternion = goog.require('goog.vec.Quaternion');
const Vec3 = goog.require('goog.vec.Vec3');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');
const vec3f = goog.require('goog.vec.vec3f');

testSuite({
  testCreateIdentityFloat32() {
    const q = Quaternion.createIdentityFloat32();
    assertElementsEquals([0, 0, 0, 1], q);
  },

  testInvert() {
    const q0 = Quaternion.createFloat32FromValues(1, 2, 3, 4);
    const q1 = Quaternion.createFloat32();

    Quaternion.invert(q0, q1);
    assertElementsRoughlyEqual([1, 2, 3, 4], q0, vec.EPSILON);
    assertElementsRoughlyEqual(
        [-0.033333, -0.066666, -0.1, 0.133333], q1, vec.EPSILON);
  },

  testConjugate() {
    const q0 = Quaternion.createFloat32FromValues(1, 2, 3, 4);
    const q1 = Quaternion.createFloat32();

    Quaternion.conjugate(q0, q1);
    assertElementsEquals([1, 2, 3, 4], q0);
    assertElementsEquals([-1, -2, -3, 4], q1);

    Quaternion.conjugate(q1, q1);
    assertElementsEquals([1, 2, 3, 4], q1);

    // Conjugate and inverse of a normalized quaternion should be equal.
    const q2 = Quaternion.createFloat32();
    const q3 = Quaternion.createFloat32();

    Quaternion.normalize(q0, q2);
    Quaternion.conjugate(q2, q2);

    Quaternion.normalize(q0, q3);
    Quaternion.invert(q3, q3);

    assertElementsRoughlyEqual(q2, q3, vec.EPSILON);
  },

  testConcat() {
    const q0 = Quaternion.createFloat32FromValues(1, 2, 3, 4);
    const q1 = Quaternion.createFloat32FromValues(2, 3, 4, 5);
    const q2 = Quaternion.createFloat32();
    Quaternion.concat(q0, q1, q2);
    assertElementsEquals([12, 24, 30, 0], q2);

    Quaternion.concat(q0, q1, q0);
    assertElementsEquals([12, 24, 30, 0], q0);
  },

  testMakeIdentity() {
    const q = Quaternion.createFloat32FromValues(1, 2, 3, 4);
    Quaternion.makeIdentity(q);
    assertElementsEquals([0, 0, 0, 1], q);
  },

  testRotateX() {
    const q = Quaternion.createIdentityFloat32();
    Quaternion.rotateX(q, Math.PI / 2, q);

    const axis = Vec3.createFloat32();
    const angle = Quaternion.toAngleAxis(q, axis);

    assertElementsRoughlyEqual([1, 0, 0], axis, vec.EPSILON);
    assertRoughlyEquals(Math.PI / 2, angle, vec.EPSILON);
  },

  testRotateY() {
    const q = Quaternion.createIdentityFloat32();
    Quaternion.rotateY(q, Math.PI / 2, q);

    const axis = Vec3.createFloat32();
    const angle = Quaternion.toAngleAxis(q, axis);

    assertElementsRoughlyEqual([0, 1, 0], axis, vec.EPSILON);
    assertRoughlyEquals(Math.PI / 2, angle, vec.EPSILON);
  },

  testRotateZ() {
    const q = Quaternion.createIdentityFloat32();
    Quaternion.rotateZ(q, Math.PI / 2, q);

    const axis = Vec3.createFloat32();
    const angle = Quaternion.toAngleAxis(q, axis);

    assertElementsRoughlyEqual([0, 0, 1], axis, vec.EPSILON);
    assertRoughlyEquals(Math.PI / 2, angle, vec.EPSILON);
  },

  testTransformVec() {
    const q = Quaternion.createIdentityFloat32();
    Quaternion.rotateX(q, Math.PI / 2, q);

    const v0 = vec3f.setFromArray(vec3f.create(), [0, 0, 1]);
    const v1 = vec3f.create();

    Quaternion.transformVec(v0, q, v1);
    assertElementsRoughlyEqual([0, -1, 0], v1, vec.EPSILON);
  },

  testSlerp() {
    const q0 = Quaternion.createFloat32FromValues(1, 2, 3, 4);
    const q1 = Quaternion.createFloat32FromValues(5, -6, 7, -8);
    const q2 = Quaternion.createFloat32();

    Quaternion.slerp(q0, q1, 0, q2);
    assertElementsEquals([5, -6, 7, -8], q2);

    Quaternion.normalize(q0, q0);
    Quaternion.normalize(q1, q1);

    Quaternion.slerp(q0, q0, .5, q2);
    assertElementsEquals(q0, q2);

    Quaternion.slerp(q0, q1, 0, q2);
    assertElementsEquals(q0, q2);

    Quaternion.slerp(q0, q1, 1, q2);
    if (q1[3] * q2[3] < 0) {
      Quaternion.negate(q2, q2);
    }
    assertElementsEquals(q1, q2);

    Quaternion.slerp(q0, q1, .3, q2);
    assertElementsRoughlyEqual(
        [-0.000501537327541, 0.4817612034640, 0.2398775270769, 0.842831337398],
        q2, vec.EPSILON);

    Quaternion.slerp(q0, q1, .5, q2);
    assertElementsRoughlyEqual(
        [-0.1243045421171, 0.51879732466, 0.0107895780990, 0.845743047108], q2,
        vec.EPSILON);

    Quaternion.slerp(q0, q1, .8, q0);
    assertElementsRoughlyEqual(
        [-0.291353561485, 0.506925588797, -0.3292443285721, 0.741442999653], q0,
        vec.EPSILON);
  },

  testFromRotMatrix() {
    const m0 = Mat3.createFloat32FromValues(
        -0.408248, 0.8796528, -0.244016935, -0.4082482, 0.06315623, 0.9106836,
        0.8164965, 0.47140452, 0.3333333);
    const q0 = Quaternion.createFloat32();
    Quaternion.fromRotationMatrix3(m0, q0);
    assertElementsRoughlyEqual(
        [
          0.22094256606638, 0.53340203646030, 0.64777022739548,
          0.497051689967954
        ],
        q0, vec.EPSILON);

    const m1 = Mat3.createFloat32FromValues(
        -0.544310, 0, 0.838884, 0, 1, 0, -0.838884, 0, -0.544310);
    const q1 = Quaternion.createFloat32();
    Quaternion.fromRotationMatrix3(m1, q1);
    assertElementsRoughlyEqual(
        [0, -0.87872350215912, 0, 0.477331042289734], q1, vec.EPSILON);

    const m2 = Mat4.createFloat32FromValues(
        -0.408248, 0.8796528, -0.244016935, 0, -0.4082482, 0.06315623,
        0.9106836, 0, 0.8164965, 0.47140452, 0.3333333, 0, 0, 0, 0, 1);
    const q2 = Quaternion.createFloat32();
    Quaternion.fromRotationMatrix4(m2, q2);
    assertElementsRoughlyEqual(
        [
          0.22094256606638, 0.53340203646030, 0.64777022739548,
          0.497051689967954
        ],
        q2, vec.EPSILON);

    const m3 = Mat4.createFloat32FromValues(
        -0.544310, 0, 0.838884, 0, 0, 1, 0, 0, -0.838884, 0, -0.544310, 0, 0, 0,
        0, 1);
    const q3 = Quaternion.createFloat32();
    Quaternion.fromRotationMatrix4(m3, q3);
    assertElementsRoughlyEqual(
        [0, -0.87872350215912, 0, 0.477331042289734], q3, vec.EPSILON);

    assertElementsRoughlyEqual(q0, q2, vec.EPSILON);
    assertElementsRoughlyEqual(q1, q3, vec.EPSILON);
  },

  testToRotMatrix() {
    const q0 = Quaternion.createFloat32FromValues(
        0.22094256606638, 0.53340203646030, 0.64777022739548,
        0.497051689967954);
    const m0 = Mat3.createFloat32();
    Quaternion.toRotationMatrix3(q0, m0);

    assertElementsRoughlyEqual(
        [
          -0.408248, 0.8796528, -0.244016935, -0.4082482, 0.06315623, 0.9106836,
          0.8164965, 0.47140452, 0.3333333
        ],
        m0, vec.EPSILON);

    const m1 = Mat4.createFloat32();
    Quaternion.toRotationMatrix4(q0, m1);

    assertElementsRoughlyEqual(
        [
          -0.408248, 0.8796528, -0.244016935, 0, -0.4082482, 0.06315623,
          0.9106836, 0, 0.8164965, 0.47140452, 0.3333333, 0, 0, 0, 0, 1
        ],
        m1, vec.EPSILON);
  },

  testToAngleAxis() {
    // Test the identity rotation.
    const q0 = Quaternion.createFloat32FromValues(0, 0, 0, 1);
    const axis = Vec3.createFloat32();
    let angle = Quaternion.toAngleAxis(q0, axis);
    assertRoughlyEquals(0.0, angle, vec.EPSILON);
    assertElementsRoughlyEqual([1, 0, 0], axis, vec.EPSILON);

    // Check equivalent representations of the same rotation.
    Quaternion.setFromValues(
        q0, -0.288675032, 0.622008682, -0.17254543, 0.70710678);
    angle = Quaternion.toAngleAxis(q0, axis);
    assertRoughlyEquals(Math.PI / 2, angle, vec.EPSILON);
    assertElementsRoughlyEqual(
        [-0.408248, 0.8796528, -0.244016], axis, vec.EPSILON);
    // The polar opposite unit quaternion is the same rotation, so we
    // check that the negated quaternion yields the negated angle and axis.
    Quaternion.negate(q0, q0);
    angle = Quaternion.toAngleAxis(q0, axis);
    assertRoughlyEquals(-Math.PI / 2, angle, vec.EPSILON);
    assertElementsRoughlyEqual(
        [0.408248, -0.8796528, 0.244016], axis, vec.EPSILON);

    // Verify that the inverse rotation yields the inverse axis.
    Quaternion.conjugate(q0, q0);
    angle = Quaternion.toAngleAxis(q0, axis);
    assertRoughlyEquals(-Math.PI / 2, angle, vec.EPSILON);
    assertElementsRoughlyEqual(
        [-0.408248, 0.8796528, -0.244016], axis, vec.EPSILON);
  },

  testFromAngleAxis() {
    // Test identity rotation (zero angle or multiples of TWO_PI).
    let angle = 0.0;
    const axis = Vec3.createFloat32FromValues(-0.408248, 0.8796528, -0.244016);
    const q0 = Quaternion.createFloat32();
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual([0, 0, 0, 1], q0, vec.EPSILON);
    angle = 4 * Math.PI;
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual([0, 0, 0, 1], q0, vec.EPSILON);

    // General test of various rotations around axes of different lengths.
    angle = Math.PI / 2;
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual(
        [-0.288675032, 0.622008682, -0.17254543, 0.70710678], q0, vec.EPSILON);
    // Angle multiples of TWO_PI with a scaled axis should be the same.
    angle += 4 * Math.PI;
    Vec3.scale(axis, 7.0, axis);
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual(
        [-0.288675032, 0.622008682, -0.17254543, 0.70710678], q0, vec.EPSILON);
    Vec3.setFromValues(axis, 1, 5, 8);
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual(
        [0.074535599, 0.372677996, 0.596284794, 0.70710678], q0, vec.EPSILON);

    // Check equivalent representations of the same rotation.
    angle = Math.PI / 5;
    Vec3.setFromValues(axis, 5, -2, -10);
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual(
        [0.136037146, -0.0544148586, -0.27207429, 0.951056516], q0,
        vec.EPSILON);
    // The negated angle and axis should yield the same rotation.
    angle = -Math.PI / 5;
    Vec3.negate(axis, axis);
    Quaternion.fromAngleAxis(angle, axis, q0);
    assertElementsRoughlyEqual(
        [0.136037146, -0.0544148586, -0.27207429, 0.951056516], q0,
        vec.EPSILON);
  },
});

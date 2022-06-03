/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.style.transformTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const transform = goog.require('goog.style.transform');
const userAgent = goog.require('goog.userAgent');

/**
 * Floating point equality tolerance.
 * @const {number}
 */
const EPSILON = .0001;

/**
 * Element being transformed.
 * @type {!Element}
 */
let element;

/**
 * Sets a transform translation and asserts the translation was applied.
 * @param {number} x The horizontal translation
 * @param {number} y The vertical translation
 */
const setAndAssertTranslation = (x, y) => {
  if (userAgent.GECKO ||
      userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
    // Mozilla and <IE10 do not support CSSMatrix.
    return;
  }
  const success = transform.setTranslation(element, x, y);
  if (!transform.isSupported()) {
    assertFalse(success);
  } else {
    assertTrue(success);
    const translation = transform.getTranslation(element);
    assertEquals(x, translation.x);
    assertEquals(y, translation.y);
  }
};

/**
 * Sets a transform translation and asserts the translation was applied.
 * @param {number} x The horizontal scale
 * @param {number} y The vertical scale
 * @param {number} z The depth scale
 */
const setAndAssertScale = (x, y, z) => {
  if (userAgent.GECKO ||
      userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
    // Mozilla and <IE10 do not support CSSMatrix.
    return;
  }
  const success = transform.setScale(element, x, y, z);
  if (!transform.isSupported()) {
    assertFalse(success);
  } else {
    assertTrue(success);
    const scale = transform.getScale(element);
    assertEquals(x, scale.x);
    assertEquals(y, scale.y);
    if (transform.is3dSupported()) {
      assertEquals(z, scale.z);
    }
  }
};

/**
 * Sets a transform rotation and asserts the translation was applied.
 * @param {number|function(number):boolean} expectedDegrees The expected
 *     resulting rotation in degrees, or a function to evaluate the resulting
 *     rotation.
 * @param {string=} opt_transform The plaintext CSS transform value.
 * @suppress {visibility} suppression added to enable type checking
 */
const setAndAssertRotation = (expectedDegrees, opt_transform) => {
  if (userAgent.GECKO ||
      userAgent.IE && !userAgent.isDocumentModeOrHigher(10)) {
    // Mozilla and <IE10 do not support CSSMatrix.
    return;
  }
  if (opt_transform) {
    style.setStyle(element, transform.getTransformProperty_(), opt_transform);
  } else {
    const success = transform.setRotation(element, Number(expectedDegrees));
    if (!transform.isSupported()) {
      assertFalse(success);
      return;
    } else {
      assertTrue(success);
    }
  }
  const rotation = transform.getRotation(element);
  if (expectedDegrees instanceof Function) {
    assertTrue(`Incorrect rotation: ${rotation}`, expectedDegrees(rotation));
  } else {
    assertRoughlyEquals(expectedDegrees, rotation, EPSILON);
  }
};

testSuite({
  setUp() {
    element = dom.createElement(TagName.DIV);
    dom.appendChild(dom.getDocument().body, element);
  },

  tearDown() {
    dom.removeNode(element);
  },

  testIsSupported() {
    if (userAgent.IE && !isVersion(9)) {
      assertFalse(transform.isSupported());
    } else {
      assertTrue(transform.isSupported());
    }
  },

  testIs3dSupported() {
    if (userAgent.GECKO && !isVersion(10) || (userAgent.IE && !isVersion(10))) {
      assertFalse(transform.is3dSupported());
    } else {
      assertTrue(transform.is3dSupported());
    }
  },

  testTranslateX() {
    setAndAssertTranslation(10, 0);
  },

  testTranslateY() {
    setAndAssertTranslation(0, 10);
  },

  testTranslateXY() {
    setAndAssertTranslation(10, 20);
  },

  testScaleX() {
    setAndAssertScale(5, 1, 1);
  },

  testScaleY() {
    setAndAssertScale(1, 3, 1);
  },

  testScaleZ() {
    setAndAssertScale(1, 1, 8);
  },

  testScale() {
    setAndAssertScale(2, 2, 2);
  },

  testRotatePositive() {
    setAndAssertRotation(90);
  },

  testRotateNegative() {
    setAndAssertRotation(-90);
  },

  testGetRotationWhenScaledUp() {
    setAndAssertRotation(90, 'scale(5) rotate3d(0,0,1,90deg)');
  },

  testGetRotationWhenScaledDown() {
    setAndAssertRotation(90, 'scale(.5) rotate3d(0,0,1,90deg)');
  },

  testGetRotationWithSkew() {
    setAndAssertRotation(0, 'skew(30deg, 30deg)');
    // NOTE: Non-zero rotations are not well-defined with a skew, but the lower
    // and upper bounds are. So check that the rotation is within these bounds.
    setAndAssertRotation((x) => x > 0 && x < 30, 'skew(0, 30deg)');
    setAndAssertRotation((x) => x < 0 && x > -30, 'skew(30deg, 0)');
  },
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.ext.PathTest');
goog.setTestOnly();

const GoogRect = goog.require('goog.math.Rect');
const Path = goog.require('goog.graphics.ext.Path');
const graphics = goog.require('goog.graphics');
const testSuite = goog.require('goog.testing.testSuite');
const testingGraphics = goog.require('goog.testing.graphics');

testSuite({
  testClone() {
    const path = new Path()
                     .moveTo(0, 0)
                     .lineTo(1, 1)
                     .curveTo(2, 2, 3, 3, 4, 4)
                     .arc(5, 5, 6, 6, 0, 90, false)
                     .close();
    assertTrue('Cloned path is a goog.graphics.ext.Path', path instanceof Path);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testBoundingBox() {
    const path =
        new Path().moveTo(0, 0).lineTo(1, 1).curveTo(2, 2, 3, 3, 4, 4).close();
    assertTrue(
        'Bounding box is correct',
        GoogRect.equals(path.getBoundingBox(), new GoogRect(0, 0, 4, 4)));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testModifyBounds() {
    const path1 =
        new Path().moveTo(0, 0).lineTo(1, 1).curveTo(2, 2, 3, 3, 4, 4).close();
    testingGraphics.assertPathEquals(
        ['M', -2, -2, 'L', 0, 0, 'C', 2, 2, 4, 4, 6, 6, 'X'],
        path1.modifyBounds(-1, -1, 2, 2));
  },
});

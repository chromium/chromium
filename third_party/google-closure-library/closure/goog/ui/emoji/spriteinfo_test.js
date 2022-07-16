/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.emoji.SpriteInfoTest');
goog.setTestOnly();

const SpriteInfo = goog.require('goog.ui.emoji.SpriteInfo');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testGetCssValues() {
    const si = new SpriteInfo(null, 'im/s.png', 10, 10, 0, 128);
    assertEquals('10px', si.getWidthCssValue());
    assertEquals('10px', si.getHeightCssValue());
    assertEquals('0', si.getXOffsetCssValue());
    assertEquals('-128px', si.getYOffsetCssValue());
  },

  testIncompletelySpecifiedSpriteInfoFails() {
    assertThrows(
        'CSS class can\'t be null if the rest of the metadata ' +
            'isn\'t specified',
        () => {
          new SpriteInfo(null);
        });

    assertThrows('Can\'t create an incompletely specified sprite info', () => {
      new SpriteInfo(null, 's.png', 10);
    });

    assertThrows('Can\'t create an incompletely specified sprite info', () => {
      new SpriteInfo(null, 's.png', 10, 10);
    });

    assertThrows('Can\'t create an incompletely specified sprite info', () => {
      new SpriteInfo(null, 's.png', 10, 10, 0);
    });
  },
});

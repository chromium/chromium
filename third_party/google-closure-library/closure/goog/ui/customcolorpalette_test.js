/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.CustomColorPaletteTest');
goog.setTestOnly();

const CustomColorPalette = goog.require('goog.ui.CustomColorPalette');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const testSuite = goog.require('goog.testing.testSuite');

let samplePalette;

testSuite({
  setUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    samplePalette = new CustomColorPalette();
  },

  tearDown() {
    samplePalette.dispose();
    document.getElementById('sandbox').innerHTML = '';
  },

  testRender() {
    samplePalette.render(document.getElementById('sandbox'));

    assertTrue('Palette must have been rendered', samplePalette.isInDocument());

    const elem = samplePalette.getElement();
    assertNotNull('The palette element should not be null', elem);
    assertEquals(
        'The palette element should have the right tag name',
        String(TagName.DIV), elem.tagName);

    assertTrue(
        'The custom color palette should have the right class name',
        classlist.contains(elem, 'goog-palette'));
  },

  testSetColors() {
    const colorSet = [
      '#e06666',
      '#f6b26b',
      '#ffd966',
      '#93c47d',
      '#76a5af',
      '#6fa8dc',
      '#8e7cc3',
    ];
    samplePalette.setColors(colorSet);
    assertSameElements(
        'The palette should have the correct set of colors', colorSet,
        samplePalette.getColors());
  },
});

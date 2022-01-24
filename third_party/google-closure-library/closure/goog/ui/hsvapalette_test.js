/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Stubbing goog.dom
 */

goog.module('goog.ui.HsvaPaletteTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const GoogEvent = goog.require('goog.events.Event');
const HsvaPalette = goog.require('goog.ui.HsvaPalette');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const colorAlpha = goog.require('goog.color.alpha');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let samplePalette;
const stubs = new PropertyReplacer();

testSuite({
  setUp() {
    samplePalette = new HsvaPalette();
  },

  tearDown() {
    samplePalette.dispose();
    stubs.reset();
  },

  testZeroAlpha() {
    const palette = new HsvaPalette(null, undefined, 0);
    assertEquals(0, palette.getAlpha());
  },

  testOptionalInitialColor() {
    const alpha = 0.5;
    const color = '#0000ff';
    const palette = new HsvaPalette(null, color, alpha);
    assertEquals(color, palette.getColor());
    assertEquals(alpha, palette.getAlpha());
  },

  testCustomClassName() {
    const customClassName = 'custom-plouf';
    /** @suppress {checkTypes} suppression added to enable type checking */
    const customClassPalette =
        new HsvaPalette(null, null, null, customClassName);
    customClassPalette.createDom();
    assertTrue(
        classlist.contains(customClassPalette.getElement(), customClassName));
  },

  testSetColor() {
    let color = '#abcdef01';
    samplePalette.setColorRgbaHex(color);
    assertEquals(color, colorAlpha.parse(samplePalette.getColorRgbaHex()).hex);
    color = 'abcdef01';
    samplePalette.setColorRgbaHex(color);
    assertEquals(
        `#${color}`, colorAlpha.parse(samplePalette.getColorRgbaHex()).hex);
  },

  testRender() {
    samplePalette.render(document.getElementById('sandbox'));

    assertTrue(samplePalette.isInDocument());

    const elem = samplePalette.getElement();
    assertNotNull(elem);
    assertEquals(String(TagName.DIV), elem.tagName);

    assertEquals(
        'The noalpha class must not be present', 'goog-hsva-palette',
        elem.className);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInputColor() {
    samplePalette.render(document.getElementById('sandbox'));
    const color = '#00112233';
    /** @suppress {visibility} suppression added to enable type checking */
    samplePalette.inputElement.value = color;
    samplePalette.handleInput(null);
    assertEquals(color, colorAlpha.parse(samplePalette.getColorRgbaHex()).hex);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleMouseMoveAlpha() {
    samplePalette.render(document.getElementById('sandbox'));
    stubs.set(goog.dom, 'getPageScroll', () => new Coordinate(0, 0));

    // Lowering the opacity of a dark, opaque red should yield a
    // more transparent red.
    samplePalette.setColorRgbaHex('#630c0000');
    style.setPageOffset(samplePalette.aImageEl_, 0, 0);
    style.setSize(samplePalette.aImageEl_, 10, 100);
    /** @suppress {visibility} suppression added to enable type checking */
    const boundaries = style.getBounds(samplePalette.aImageEl_);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const event = new GoogEvent();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.clientY = boundaries.top;
    samplePalette.handleMouseMoveA_(boundaries, event);

    assertEquals('#630c00ff', samplePalette.getColorRgbaHex());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSwatchOpacity() {
    samplePalette.render(document.getElementById('sandbox'));

    samplePalette.setAlpha(1);
    assertEquals(1, style.getOpacity(samplePalette.swatchElement));

    samplePalette.setAlpha(0x99 / 0xff);
    assertEquals(0.6, style.getOpacity(samplePalette.swatchElement));

    samplePalette.setAlpha(0);
    assertEquals(0, style.getOpacity(samplePalette.swatchElement));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testNoTransparencyBehavior() {
    samplePalette.render(document.getElementById('sandbox'));

    /** @suppress {visibility} suppression added to enable type checking */
    samplePalette.inputElement.value = '#abcdef22';
    samplePalette.handleInput(null);
    /** @suppress {visibility} suppression added to enable type checking */
    samplePalette.inputElement.value = '#abcdef';
    samplePalette.handleInput(null);
    assertEquals(1, style.getOpacity(samplePalette.swatchElement));
  },
});

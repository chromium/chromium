/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.SvgGraphicsTest');
goog.setTestOnly();

const AffineTransform = goog.require('goog.graphics.AffineTransform');
const SolidFill = goog.require('goog.graphics.SolidFill');
const SvgGraphics = goog.require('goog.graphics.SvgGraphics');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let graphics;

testSuite({
  setUp() {
    if (!document.createElementNS) {
      // Some browsers don't support document.createElementNS and this test
      // should not be run on those browsers (IE7,8).
      return;
    }
    graphics = new SvgGraphics('100px', '100px');
    graphics.createDom();
    dom.getElement('root').appendChild(graphics.getElement());
  },

  testAddDef() {
    if (!graphics) {
      // setUp has failed (no browser support), we should not run this test.
      return;
    }
    const defElement1 = document.createElement('div');
    const defElement2 = document.createElement('div');
    const defKey1 = 'def1';
    const defKey2 = 'def2';
    let id = graphics.addDef(defKey1, defElement1);
    assertEquals('_svgdef_0', id);
    id = graphics.addDef(defKey1, defElement2);
    assertEquals('_svgdef_0', id);
    id = graphics.addDef(defKey2, defElement2);
    assertEquals('_svgdef_1', id);
  },

  testGetDef() {
    if (!graphics) {
      // setUp has failed (no browser support), we should not run this test.
      return;
    }
    const defElement = document.createElement('div');
    const defKey = 'def';
    const id = graphics.addDef(defKey, defElement);
    assertEquals(id, graphics.getDef(defKey));
    assertNull(graphics.getDef('randomKey'));
  },

  testRemoveDef() {
    if (!graphics) {
      // setUp has failed (no browser support), we should not run this test.
      return;
    }
    const defElement = document.createElement('div');
    const defKey = 'def';
    const addedId = graphics.addDef(defKey, defElement);
    graphics.removeDef('randomKey');
    assertEquals(addedId, graphics.getDef(defKey));
    graphics.removeDef(defKey);
    assertNull(graphics.getDef(defKey));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSetElementAffineTransform() {
    if (!graphics) {
      // setUp has failed (no browser support), we should not run this test.
      return;
    }
    const fill = new SolidFill('blue');
    const stroke = null;
    const rad = -3.1415926 / 6;
    const costheta = Math.cos(rad);
    const sintheta = Math.sin(rad);
    const dx = 10;
    const dy = -20;
    const affine = new AffineTransform(
        costheta, -sintheta + 1, sintheta, costheta, dx, dy);
    const rect = graphics.drawRect(10, 20, 30, 40, stroke, fill);
    rect.setTransform(affine);
    graphics.render();
    // getTransformToElement was deleted in Chrome48. See
    // https://code.google.com/p/chromium/issues/detail?id=524432.
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    function getTransformToElement(element, target) {
      return target.getScreenCTM().inverse().multiply(element.getScreenCTM());
    }
    const svgMatrix =
        getTransformToElement(rect.getElement(), graphics.getElement());
    assertRoughlyEquals(svgMatrix.a, costheta, 0.001);
    assertRoughlyEquals(svgMatrix.b, -sintheta + 1, 0.001);
    assertRoughlyEquals(svgMatrix.c, sintheta, 0.001);
    assertRoughlyEquals(svgMatrix.d, costheta, 0.001);
    assertRoughlyEquals(svgMatrix.e, dx, 0.001);
    assertRoughlyEquals(svgMatrix.f, dy, 0.001);
  },
});

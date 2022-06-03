/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.CanvasGraphicsTest');
goog.setTestOnly();

const CanvasGraphics = goog.require('goog.graphics.CanvasGraphics');
const SolidFill = goog.require('goog.graphics.SolidFill');
const Stroke = goog.require('goog.graphics.Stroke');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let graphics;

/** @suppress {missingProperties} suppression added to enable type checking */
function shouldRunTests() {
  graphics = new CanvasGraphics(100, 100);
  graphics.createDom();
  return graphics.canvas_.getContext;
}

testSuite({
  setUp() {
    graphics = new CanvasGraphics(100, 100);
    graphics.createDom();
    dom.getElement('root').appendChild(graphics.getElement());
    graphics.enterDocument();
  },

  tearDown() {
    graphics.dispose();
    dom.removeNode(graphics.getElement());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDrawRemoveRect() {
    const fill = new SolidFill('red');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const stroke = new Stroke('blue');
    const element = graphics.drawRect(10, 10, 80, 80, stroke, fill);
    assertEquals(1, graphics.canvasElement.children_.length);
    graphics.removeElement(element);
    assertEquals(0, graphics.canvasElement.children_.length);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDrawRemoveNestedRect() {
    const fill = new SolidFill('red');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const stroke = new Stroke('blue');
    const group = graphics.createGroup();
    assertEquals(1, graphics.canvasElement.children_.length);
    assertEquals(0, graphics.canvasElement.children_[0].children_.length);
    const element = graphics.drawRect(10, 10, 80, 80, stroke, fill, group);
    assertEquals(1, graphics.canvasElement.children_.length);
    assertEquals(1, graphics.canvasElement.children_[0].children_.length);
    graphics.removeElement(element);
    assertEquals(1, graphics.canvasElement.children_.length);
    assertEquals(0, graphics.canvasElement.children_[0].children_.length);
  },
});

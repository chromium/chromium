/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.ext.ElementTest');
goog.setTestOnly();

const StrictMock = goog.require('goog.testing.StrictMock');
const ext = goog.require('goog.graphics.ext');
const googGraphics = goog.require('goog.graphics');
const testSuite = goog.require('goog.testing.testSuite');

let el;
let graphics;
let mockWrapper;

/** @suppress {missingProperties} suppression added to enable type checking */
function assertPosition(fn, left, top, width = undefined, height = undefined) {
  mockWrapper.setTransformation(0, 0, 0, 5, 5);
  mockWrapper.setTransformation(
      left, top, 0, (width || 10) / 2, (height || 10) / 2);
  mockWrapper.$replay();

  /** @suppress {checkTypes} suppression added to enable type checking */
  el = new ext.Element(graphics, mockWrapper);
  el.setSize(10, 10);
  fn();
}

testSuite({
  setUp() {
    const div = document.getElementById('root');
    graphics = new ext.Graphics(100, 100, 200, 200);
    div.textContent = '';
    graphics.render(div);

    mockWrapper = new StrictMock(googGraphics.Element);
  },

  tearDown() {
    mockWrapper.$verify();
  },

  testLeft() {
    assertPosition(() => {
      el.setLeft(10);
    }, 10, 0);
    assertFalse(el.isParentDependent());
  },

  testLeftPercent() {
    assertPosition(() => {
      el.setLeft('10%');
    }, 20, 0);
  },

  testCenter() {
    assertPosition(() => {
      el.setCenter(0);
    }, 95, 0);
    assertTrue(el.isParentDependent());
  },

  testCenterPercent() {
    assertPosition(() => {
      el.setCenter('10%');
    }, 115, 0);
  },

  testRight() {
    assertPosition(() => {
      el.setRight(10);
    }, 180, 0);
    assertTrue(el.isParentDependent());
  },

  testRightPercent() {
    assertPosition(() => {
      el.setRight('10%');
    }, 170, 0);
    assertTrue(el.isParentDependent());
  },

  testTop() {
    assertPosition(() => {
      el.setTop(10);
    }, 0, 10);
    assertFalse(el.isParentDependent());
  },

  testTopPercent() {
    assertPosition(() => {
      el.setTop('10%');
    }, 0, 20);
  },

  testMiddle() {
    assertPosition(() => {
      el.setMiddle(0);
    }, 0, 95);
    assertTrue(el.isParentDependent());
  },

  testMiddlePercent() {
    assertPosition(() => {
      el.setMiddle('10%');
    }, 0, 115);
  },

  testBottom() {
    assertPosition(() => {
      el.setBottom(10);
    }, 0, 180);
    assertTrue(el.isParentDependent());
  },

  testBottomPercent() {
    assertPosition(() => {
      el.setBottom('10%');
    }, 0, 170);
    assertTrue(el.isParentDependent());
  },

  testSize() {
    assertPosition(() => {
      el.setSize(100, 100);
    }, 0, 0, 100, 100);
    assertFalse(el.isParentDependent());
  },

  testSizePercent() {
    assertPosition(() => {
      el.setSize('10%', '20%');
    }, 0, 0, 20, 40);
    assertTrue(el.isParentDependent());
  },
});

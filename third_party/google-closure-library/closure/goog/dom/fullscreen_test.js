/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.fullscreen_test');
goog.setTestOnly();

const DomHelper = goog.require('goog.dom.DomHelper');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const asserts = goog.require('goog.testing.asserts');
const fullscreen = goog.require('goog.dom.fullscreen');
const testSuite = goog.require('goog.testing.testSuite');

let domHelper;
let mockDoc;
let stubs;

testSuite({
  setUp() {
    mockDoc = {};
    domHelper = new DomHelper();
    stubs = new PropertyReplacer();
    stubs.replace(domHelper, 'getDocument', () => mockDoc);
  },

  testGetFullScreenElement() {
    const element = document.createElement('div');
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockDoc.fullscreenElement = element;
    assertEquals(element, fullscreen.getFullScreenElement(domHelper));
  },

  testGetFullScreenElementNotFullScreen() {
    assertNull(fullscreen.getFullScreenElement(domHelper));
  },
});

/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.MenuSeparatorRendererTest');
goog.setTestOnly();

const MenuSeparator = goog.require('goog.ui.MenuSeparator');
const MenuSeparatorRenderer = goog.require('goog.ui.MenuSeparatorRenderer');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let originalSandbox;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    originalSandbox = sandbox.cloneNode(true);
  },

  tearDown() {
    sandbox.parentNode.replaceChild(originalSandbox, sandbox);
  },

  testDecorate() {
    const separator = new MenuSeparator();
    const dummyId = 'foo';
    separator.setId(dummyId);
    assertEquals(dummyId, separator.getId());
    const renderer = new MenuSeparatorRenderer();
    renderer.decorate(separator, dom.getElement('separator'));
    assertEquals('separator', separator.getId());
  },
});

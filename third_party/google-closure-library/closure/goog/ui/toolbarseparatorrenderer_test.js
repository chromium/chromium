/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ToolbarSeparatorRendererTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const INLINE_BLOCK_CLASSNAME = goog.require('goog.ui.INLINE_BLOCK_CLASSNAME');
const TagName = goog.require('goog.dom.TagName');
const ToolbarSeparator = goog.require('goog.ui.ToolbarSeparator');
const ToolbarSeparatorRenderer = goog.require('goog.ui.ToolbarSeparatorRenderer');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let parent;
let renderer;
let separator;

testSuite({
  setUp() {
    parent = dom.getElement('parent');
    renderer = ToolbarSeparatorRenderer.getInstance();
    separator = new ToolbarSeparator(renderer);
  },

  tearDown() {
    separator.dispose();
    dom.removeChildren(parent);
  },

  testConstructor() {
    assertNotNull('Renderer must not be null', renderer);
  },

  testGetCssClass() {
    assertEquals(
        'getCssClass() must return expected value',
        ToolbarSeparatorRenderer.CSS_CLASS, renderer.getCssClass());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCreateDom() {
    const element = renderer.createDom(separator);
    assertNotNull('Created element must not be null', element);
    assertEquals(
        'Created element must be a DIV', String(TagName.DIV), element.tagName);
    assertSameElements(
        'Created element must have expected class names',
        [
          ToolbarSeparatorRenderer.CSS_CLASS,
          // Separators are always in a disabled state.
          renderer.getClassForState(Component.State.DISABLED),
          INLINE_BLOCK_CLASSNAME,
        ],
        classlist.get(element));
  },

  testCreateDomWithExtraCssClass() {
    separator.addClassName('another-class');
    const element = renderer.createDom(separator);
    assertContains(
        'Created element must contain extra CSS classes', 'another-class',
        classlist.get(element));
  },
});

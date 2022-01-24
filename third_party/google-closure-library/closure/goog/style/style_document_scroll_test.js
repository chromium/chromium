/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.style.style_document_scroll_test');
goog.setTestOnly();

const dom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

const EPSILON = 2;
let documentScroll;

testSuite({
  setUp() {
    documentScroll = dom.getDocumentScrollElement();
    documentScroll.scrollTop = 100;
    documentScroll.scrollLeft = 100;
  },

  tearDown() {
    documentScroll.style.border = '';
    documentScroll.style.padding = '';
    documentScroll.style.margin = '';
    documentScroll.scrollTop = 0;
    documentScroll.scrollLeft = 0;
  },

  testDocumentScrollWithZeroedBodyProperties() {
    assertRoughlyEquals(
        200, style.getContainerOffsetToScrollInto(dom.getElement('testEl1')).y,
        EPSILON);
    assertRoughlyEquals(
        300, style.getContainerOffsetToScrollInto(dom.getElement('testEl2')).x,
        EPSILON);
  },

  testDocumentScrollWithMargin() {
    documentScroll.style.margin = '20px 0 0 30px';
    assertRoughlyEquals(
        220, style.getContainerOffsetToScrollInto(dom.getElement('testEl1')).y,
        EPSILON);
    assertRoughlyEquals(
        330, style.getContainerOffsetToScrollInto(dom.getElement('testEl2')).x,
        EPSILON);
  },

  testDocumentScrollWithPadding() {
    documentScroll.style.padding = '20px 0 0 30px';
    assertRoughlyEquals(
        220, style.getContainerOffsetToScrollInto(dom.getElement('testEl1')).y,
        EPSILON);
    assertRoughlyEquals(
        330, style.getContainerOffsetToScrollInto(dom.getElement('testEl2')).x,
        EPSILON);
  },

  testDocumentScrollWithBorder() {
    documentScroll.style.border = '20px solid green';
    assertRoughlyEquals(
        220, style.getContainerOffsetToScrollInto(dom.getElement('testEl1')).y,
        EPSILON);
    assertRoughlyEquals(
        320, style.getContainerOffsetToScrollInto(dom.getElement('testEl2')).x,
        EPSILON);
  },

  testDocumentScrollWithAllProperties() {
    documentScroll.style.margin = '20px 0 0 30px';
    documentScroll.style.padding = '40px 0 0 50px';
    documentScroll.style.border = '10px solid green';
    assertRoughlyEquals(
        270, style.getContainerOffsetToScrollInto(dom.getElement('testEl1')).y,
        EPSILON);
    assertRoughlyEquals(
        390, style.getContainerOffsetToScrollInto(dom.getElement('testEl2')).x,
        EPSILON);
  },

  testDocumentScrollNoOpIfElementAlreadyInView() {
    // Scroll once to make testEl3 visible.
    documentScroll.scrollTop =
        style.getContainerOffsetToScrollInto(dom.getElement('testEl3')).y;

    // Scroll a bit more so that now the element is approximately at the middle.
    const viewportHeight = documentScroll.clientHeight;
    documentScroll.scrollTop += viewportHeight / 2;

    // Since the element is fully within viewport, additional calls to
    // getContainerOffsetToScrollInto should be a no-op.
    assertEquals(
        documentScroll.scrollTop,
        style.getContainerOffsetToScrollInto(dom.getElement('testEl3')).y);
  },

  testScrollIntoContainerView() {
    style.scrollIntoContainerView(dom.getElement('testEl1'));
    assertRoughlyEquals(200, documentScroll.scrollTop, EPSILON);
    style.scrollIntoContainerView(dom.getElement('testEl2'));
    assertRoughlyEquals(300, documentScroll.scrollLeft, EPSILON);
  },
});

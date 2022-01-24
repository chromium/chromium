/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.DimensionPickerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const DimensionPicker = goog.require('goog.ui.DimensionPicker');
const DimensionPickerRenderer = goog.require('goog.ui.DimensionPickerRenderer');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Size = goog.require('goog.math.Size');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');

let picker;
let render;
let decorate;

testSuite({
  setUpPage() {
    render = dom.getElement('render');
    decorate = dom.getElement('decorate');
  },

  setUp() {
    picker = new DimensionPicker();
    dom.removeChildren(render);
    dom.removeChildren(decorate);
  },

  tearDown() {
    picker.dispose();
  },

  testConstructor() {
    assertNotNull('Should have successful construction', picker);
    assertNull('Should not be in document', picker.getElement());
  },

  testRender() {
    picker.render(render);

    assertEquals('Should create 1 child', 1, render.childNodes.length);
    assertEquals(
        'Should be a div', String(TagName.DIV), render.firstChild.tagName);
  },

  testDecorate() {
    picker.decorate(decorate);

    assertNotEquals(
        'Should add several children', decorate.firstChild, decorate.lastChild);
  },

  testHighlightedSize() {
    picker.render(render);

    let size = picker.getValue();
    assertEquals('Should have 1 column highlighted initially.', 1, size.width);
    assertEquals('Should have 1 row highlighted initially.', 1, size.height);

    picker.setValue(1, 2);
    size = picker.getValue();
    assertEquals('Should have 1 column highlighted.', 1, size.width);
    assertEquals('Should have 2 rows highlighted.', 2, size.height);

    picker.setValue(new Size(3, 4));
    size = picker.getValue();
    assertEquals('Should have 3 columns highlighted.', 3, size.width);
    assertEquals('Should have 4 rows highlighted.', 4, size.height);

    picker.setValue(new Size(-3, 0));
    size = picker.getValue();
    assertEquals(
        'Should have 1 column highlighted when passed a negative ' +
            'column value.',
        1, size.width);
    assertEquals(
        'Should have 1 row highlighted when passed 0 as the row ' +
            'value.',
        1, size.height);

    picker.setValue(picker.maxColumns + 10, picker.maxRows + 2);
    size = picker.getValue();
    assertEquals(
        'Column value should be decreased to match max columns ' +
            'if it is too high.',
        picker.maxColumns, size.width);
    assertEquals(
        'Row value should be decreased to match max rows ' +
            'if it is too high.',
        picker.maxRows, size.height);
  },

  testSizeShown() {
    picker.render(render);

    let size = picker.getSize();
    assertEquals('Should have 5 columns visible', 5, size.width);
    assertEquals('Should have 5 rows visible', 5, size.height);

    picker.setValue(4, 4);
    size = picker.getSize();
    assertEquals('Should have 5 columns visible', 5, size.width);
    assertEquals('Should have 5 rows visible', 5, size.height);

    picker.setValue(12, 13);
    size = picker.getSize();
    assertEquals('Should have 13 columns visible', 13, size.width);
    assertEquals('Should have 14 rows visible', 14, size.height);

    picker.setValue(20, 20);
    size = picker.getSize();
    assertEquals('Should have 20 columns visible', 20, size.width);
    assertEquals('Should have 20 rows visible', 20, size.height);

    picker.setValue(2, 3);
    size = picker.getSize();
    assertEquals('Should have 5 columns visible', 5, size.width);
    assertEquals('Should have 5 rows visible', 5, size.height);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleMove() {
    picker.render(render);
    const renderer = picker.getRenderer();
    const mouseMoveElem = renderer.getMouseMoveElement(picker);

    /** @suppress {visibility} suppression added to enable type checking */
    picker.rightToLeft_ = false;
    const e = {
      target: mouseMoveElem,
      offsetX: 18,  // Each grid square currently a magic 18px.
      offsetY: 36,
    };

    picker.handleMouseMove(e);
    let size = picker.getValue();
    assertEquals('Should have 1 column highlighted', 1, size.width);
    assertEquals('Should have 2 rows highlighted', 2, size.height);

    /** @suppress {visibility} suppression added to enable type checking */
    picker.rightToLeft_ = true;

    picker.handleMouseMove(e);
    size = picker.getValue();
    // In RTL we pick from the right side of the picker, so an offsetX of 0
    // would actually mean select all columns.
    assertEquals(
        'Should have columns to the right of the mouse highlighted',
        Math.ceil((mouseMoveElem.offsetWidth - e.offsetX) / 18), size.width);
    assertEquals('Should have 2 rows highlighted', 2, size.height);
  },

  testHandleKeyboardEvents() {
    picker.render(render);

    /** @suppress {visibility} suppression added to enable type checking */
    picker.rightToLeft_ = false;

    let result = picker.handleKeyEvent({keyCode: KeyCodes.DOWN});
    let size = picker.getValue();
    assertEquals('Should have 1 column highlighted', 1, size.width);
    assertEquals('Should have 2 rows highlighted', 2, size.height);
    assertTrue('Should handle DOWN key event', result);

    result = picker.handleKeyEvent({keyCode: KeyCodes.RIGHT});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 2, size.width);
    assertEquals('Should have 2 rows highlighted', 2, size.height);
    assertTrue('Should handle RIGHT key event', result);

    result = picker.handleKeyEvent({keyCode: KeyCodes.UP});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 2, size.width);
    assertEquals('Should have 1 rows highlighted', 1, size.height);
    assertTrue('Should handle UP key event', result);

    // Pressing UP when there is only 1 row should be handled but has no
    // effect.
    result = picker.handleKeyEvent({keyCode: KeyCodes.UP});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 2, size.width);
    assertEquals('Should have 1 rows highlighted', 1, size.height);
    assertTrue('Should handle UP key event', result);

    result = picker.handleKeyEvent({keyCode: KeyCodes.LEFT});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 1, size.width);
    assertEquals('Should have 1 rows highlighted', 1, size.height);
    assertTrue('Should handle LEFT key event', result);

    // Pressing LEFT when there is only 1 row should not be handled
    // allowing SubMenu to close.
    result = picker.handleKeyEvent({keyCode: KeyCodes.LEFT});
    assertFalse(
        'Should not handle LEFT key event when there is only one column',
        result);

    /** @suppress {visibility} suppression added to enable type checking */
    picker.rightToLeft_ = true;

    // In RTL the roles of the LEFT and RIGHT keys are swapped.
    result = picker.handleKeyEvent({keyCode: KeyCodes.LEFT});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 2, size.width);
    assertEquals('Should have 2 rows highlighted', 1, size.height);
    assertTrue('Should handle LEFT key event', result);

    result = picker.handleKeyEvent({keyCode: KeyCodes.RIGHT});
    size = picker.getValue();
    assertEquals('Should have 2 column highlighted', 1, size.width);
    assertEquals('Should have 1 rows highlighted', 1, size.height);
    assertTrue('Should handle RIGHT key event', result);

    result = picker.handleKeyEvent({keyCode: KeyCodes.RIGHT});
    assertFalse(
        'Should not handle RIGHT key event when there is only one column',
        result);
  },

  testDispose() {
    const element = picker.getElement();
    picker.render(render);
    picker.dispose();
    assertTrue('Picker should have been disposed of', picker.isDisposed());
    assertNull(
        'Picker element reference should have been nulled out',
        picker.getElement());
  },

  testRendererDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(
        DimensionPickerRenderer);
  },

  testSetAriaLabel() {
    assertNull(
        'Picker must not have aria label by default', picker.getAriaLabel());
    picker.setAriaLabel('My picker');
    picker.render(render);
    const element = picker.getElementStrict();
    assertNotNull('Element must not be null', element);
    assertEquals(
        'Picker element must have expected aria-label', 'My picker',
        element.getAttribute('aria-label'));
    assertTrue(dom.isFocusableTabIndex(element));
    picker.setAriaLabel('My new picker');
    assertEquals(
        'Picker element must have updated aria-label', 'My new picker',
        element.getAttribute('aria-label'));
  },

  testHandleTouchTapInsideGrid() {
    picker.setPointerEventsEnabled(true);
    picker.render(render);

    const renderer = picker.getRenderer();
    const mouseMoveElem = renderer.getMouseMoveElement(picker);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const e = new BrowserEvent({
      target: mouseMoveElem,
      offsetX: 18,  // Each grid square currently a magic 18px.
      offsetY: 36,
      clientX: 8 + 18,
      clientY: 8 + 36,
      pointerType: BrowserEvent.PointerType.TOUCH,
    });

    picker.handleMouseDown(e);
    picker.handleMouseUp(e);

    const size = picker.getValue();
    assertEquals('Should have 1 column highlighted', 1, size.width);
    assertEquals('Should have 2 rows highlighted', 2, size.height);
  },

  testHandleTouchTapOutsideGrid() {
    picker.setPointerEventsEnabled(true);
    picker.render(render);

    const renderer = picker.getRenderer();
    const mouseMoveElem = renderer.getMouseMoveElement(picker);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const e = new BrowserEvent({
      target: mouseMoveElem,
      offsetX: 18,   // Each grid square currently a magic 18px.
      offsetY: 108,  // 6th column is at 108px (6 * 18px)
      clientX: 8 + 18,
      clientY: 8 + 108,
      pointerType: BrowserEvent.PointerType.TOUCH,
    });

    picker.handleMouseDown(e);
    picker.handleMouseUp(e);

    const size = picker.getValue();
    assertEquals('Should have 1 column highlighted', 1, size.width);
    assertEquals('Should have 1 rows highlighted', 1, size.height);
  },
});

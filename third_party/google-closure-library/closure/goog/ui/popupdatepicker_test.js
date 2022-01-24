/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PopupDatePickerTest');
goog.setTestOnly();

const DateDate = goog.require('goog.date.Date');
const DatePicker = goog.require('goog.ui.DatePicker');
const MockControl = goog.require('goog.testing.MockControl');
const PopupBase = goog.require('goog.ui.PopupBase');
const PopupDatePicker = goog.require('goog.ui.PopupDatePicker');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let mockControl;
let popupDatePicker;

testSuite({
  setUp() {
    mockControl = new MockControl();
    popupDatePicker = new PopupDatePicker();
  },

  tearDown() {
    popupDatePicker.dispose();
    mockControl.$tearDown();
  },

  testIsVisible() {
    assertFalse(popupDatePicker.isVisible());
    popupDatePicker.createDom();
    assertFalse(popupDatePicker.isVisible());
    popupDatePicker.render();
    assertFalse(popupDatePicker.isVisible());
    popupDatePicker.showPopup(document.body);
    assertTrue(popupDatePicker.isVisible());
    popupDatePicker.hidePopup();
    assertFalse(popupDatePicker.isVisible());
  },

  testFiresShowAndHideEvents() {
    const showHandler = recordFunction();
    const hideHandler = recordFunction();
    events.listen(popupDatePicker, PopupBase.EventType.SHOW, showHandler);
    events.listen(popupDatePicker, PopupBase.EventType.HIDE, hideHandler);
    popupDatePicker.createDom();
    popupDatePicker.render();
    assertEquals(0, showHandler.getCallCount());
    assertEquals(0, hideHandler.getCallCount());

    popupDatePicker.showPopup(document.body);
    // Bug in goog.ui.Popup: the SHOW event is fired twice.
    assertEquals(2, showHandler.getCallCount());
    assertEquals(0, hideHandler.getCallCount());
    showHandler.reset();

    popupDatePicker.hidePopup();
    assertEquals(0, showHandler.getCallCount());
    assertEquals(1, hideHandler.getCallCount());
  },

  testShow() {
    popupDatePicker.createDom();
    popupDatePicker.render();
    const datePicker = popupDatePicker.getDatePicker();
    const date = new DateDate();

    // Date should be overwritten when opt_keepDate not specified.
    datePicker.setDate(date);
    popupDatePicker.showPopup(document.body, undefined /* opt_keepDate */);
    assertNull(datePicker.getDate());

    // Date should be overwritten when opt_keepDate is false.
    datePicker.setDate(date);
    popupDatePicker.showPopup(document.body, false /* opt_keepDate */);
    assertNull(datePicker.getDate());

    // Date should be preserved when opt_keepDate is true.
    datePicker.setDate(date);
    popupDatePicker.showPopup(document.body, true /* opt_keepDate */);
    assertTrue(date.equals(datePicker.getDate()));
  },

  /**
   * Tests that repositioning based on DatePicker growth happens if the flag is
   * enabled.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRepositioning_whenDatePickerGrows_withFlagEnabled() {
    // Given a PopupDatePicker showing with KeepAllWeeksInViewport = true.
    popupDatePicker.setKeepAllWeeksInViewport(true);
    popupDatePicker.createDom();
    popupDatePicker.render();
    const datePicker = popupDatePicker.getDatePicker();
    const date = new DateDate();

    datePicker.setDate(date);
    popupDatePicker.showPopup(document.body, undefined /* opt_keepDate */);

    mockControl.createMethodMock(popupDatePicker.popup_, 'reposition');

    // Expect the PopupDatePicker to reposition.
    popupDatePicker.popup_.reposition().$once();

    mockControl.$replayAll();

    // When the DatePicker reports a size increase.
    datePicker.dispatchEvent(DatePicker.Events.GRID_SIZE_INCREASE);
    mockControl.$verifyAll();
  },

  /**
   * Tests that repositioning based on DatePicker growth does not happen if the
   * flag is disabled.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRepositioning_whenDatePickerGrows_withFlagDisabled() {
    // Given a PopupDatePicker showing with KeepAllWeeksInViewport = false,
    // default state.
    popupDatePicker.createDom();
    popupDatePicker.render();
    const datePicker = popupDatePicker.getDatePicker();
    const date = new DateDate();

    datePicker.setDate(date);
    popupDatePicker.showPopup(document.body, undefined /* opt_keepDate */);

    mockControl.createMethodMock(popupDatePicker.popup_, 'reposition');

    // Expect the PopupDatePicker not to reposition.
    popupDatePicker.popup_.reposition().$never();

    mockControl.$replayAll();

    // When the DatePicker reports a size increase.
    datePicker.dispatchEvent(DatePicker.Events.GRID_SIZE_INCREASE);
    mockControl.$verifyAll();
  },

  testPositioning_whenReferenceElementAtBottom_pickerRendersFullyAbove() {
    // Given a PopupDatePicker rendered with relation to a reference element
    // positioned at the bottom of the screen.
    const referenceElement = dom.createElement(TagName.DIV);
    referenceElement.style.position = 'absolute';
    referenceElement.style.bottom = '0';
    referenceElement.style.height = '1em';

    document.body.style.height = '100%';
    document.body.appendChild(referenceElement);

    popupDatePicker.createDom();
    popupDatePicker.render();
    popupDatePicker.showPopup(referenceElement, undefined /* opt_keepDate */);

    const datePickerElement = popupDatePicker.getElement();

    // Expect the picker to be rendered above the reference.
    const datePickerRect = style.getBounds(datePickerElement);
    const referenceRect = style.getBounds(referenceElement);
    assertTrue(
        'Date picker should render above reference element',
        datePickerRect.top + datePickerRect.height <= referenceRect.top);

    // Clean up.
    dom.removeNode(referenceElement);
    document.body.style.height = '';
  }
});

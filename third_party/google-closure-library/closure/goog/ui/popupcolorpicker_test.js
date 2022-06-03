/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PopupColorPickerTest');
goog.setTestOnly();

const ColorPicker = goog.require('goog.ui.ColorPicker');
const PopupColorPicker = goog.require('goog.ui.PopupColorPicker');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

// Unittest to ensure that the popup gets created in createDom().

// Unittest to ensure the popup opens with a custom color picker.

testSuite({
  testPopupCreation() {
    const picker = new PopupColorPicker();
    picker.createDom();
    assertNotNull(picker.getPopup());
  },

  testAutoHideIsSetProperly() {
    const picker = new PopupColorPicker();
    picker.createDom();
    picker.setAutoHide(true);
    const containingDiv = dom.getElement('containingDiv');
    picker.setAutoHideRegion(containingDiv);
    assertTrue(picker.getAutoHide());
    assertEquals(containingDiv, picker.getAutoHideRegion());
  },

  testCustomColorPicker() {
    const button1 = document.getElementById('button1');
    const domHelper = dom.getDomHelper();
    const colorPicker = new ColorPicker();
    colorPicker.setColors(['#ffffff', '#000000']);
    const picker = new PopupColorPicker(domHelper, colorPicker);
    picker.render();
    picker.attach(button1);
    assertNotNull(picker.getColorPicker());
    assertNotNull(picker.getPopup().getElement());
    assertNull(picker.getSelectedColor());

    let changeEvents = 0;
    events.listen(picker, ColorPicker.EventType.CHANGE, (e) => {
      changeEvents++;
    });

    // Select the first color.
    testingEvents.fireClickSequence(button1);
    testingEvents.fireClickSequence(
        document.getElementById('goog-palette-cell-0').firstChild);
    assertEquals('#ffffff', picker.getSelectedColor());
    assertEquals(1, changeEvents);
  },
});

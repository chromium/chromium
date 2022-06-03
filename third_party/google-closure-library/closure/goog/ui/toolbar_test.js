/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ToolbarTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const Toolbar = goog.require('goog.ui.Toolbar');
const ToolbarMenuButton = goog.require('goog.ui.ToolbarMenuButton');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

let toolbar;
let toolbarWrapper;
let buttons;

testSuite({
  setUp() {
    toolbar = new Toolbar();
    toolbarWrapper = dom.getElement('toolbar-wrapper');

    // Render and populate the toolbar.
    toolbar.render(toolbarWrapper);
    const toolbarElem = toolbar.getElement();
    const button1 = new ToolbarMenuButton('button 1');
    const button2 = new ToolbarMenuButton('button 2');
    const button3 = new ToolbarMenuButton('button 3');
    button1.render(toolbarElem);
    button2.render(toolbarElem);
    button3.render(toolbarElem);
    toolbar.addChild(button1);
    toolbar.addChild(button2);
    toolbar.addChild(button3);
    buttons = [button1, button2, button3];
  },

  tearDown() {
    toolbar.dispose();
  },

  testHighlightFirstOnFocus() {
    const firstButton = buttons[0];

    // Verify that focusing the toolbar via the keyboard (i.e. no click event)
    // highlights the first item and sets it as the active descendant.
    events.fireFocusEvent(toolbar.getElement());
    assertEquals(0, toolbar.getHighlightedIndex());
    assertTrue(firstButton.isHighlighted());
    assertEquals(
        firstButton.getElement(),
        aria.getActiveDescendant(toolbar.getElement()));

    // Verify that removing focus unhighlights the first item and removes it as
    // the active descendant.
    events.fireBlurEvent(toolbar.getElement());
    assertEquals(-1, toolbar.getHighlightedIndex());
    assertNull(aria.getActiveDescendant(toolbar.getElement()));
    assertFalse(firstButton.isHighlighted());
  },

  testHighlightSelectedOnClick() {
    const firstButton = buttons[0];
    const secondButton = buttons[1];

    // Verify that mousing over and clicking on a toolbar button selects only
    // the correct item.
    const mouseover =
        new GoogTestingEvent(EventType.MOUSEOVER, secondButton.getElement());
    events.fireBrowserEvent(mouseover);
    const mousedown =
        new GoogTestingEvent(EventType.MOUSEDOWN, toolbar.getElement());
    events.fireBrowserEvent(mousedown);
    assertEquals(1, toolbar.getHighlightedIndex());
    assertTrue(secondButton.isHighlighted());
    assertFalse(firstButton.isHighlighted());
    assertEquals(
        secondButton.getElement(),
        aria.getActiveDescendant(toolbar.getElement()));
  },
});

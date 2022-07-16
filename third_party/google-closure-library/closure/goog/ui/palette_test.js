/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PaletteTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Container = goog.require('goog.ui.Container');
const EventType = goog.require('goog.events.EventType');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyEvent = goog.require('goog.events.KeyEvent');
const Palette = goog.require('goog.ui.Palette');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const googEvents = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let palette;
let nodes;

testSuite({
  setUp() {
    nodes = [];
    for (let i = 0; i < 23; i++) {
      const node = dom.createTextNode(`node[${i}]`);
      nodes.push(node);
    }
    palette = new Palette(nodes);
  },

  tearDown() {
    palette.dispose();
    dom.removeChildren(document.getElementById('sandbox'));
  },

  testAfterHighlightListener() {
    palette.setHighlightedIndex(0);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const handler = new recordFunction();
    googEvents.listen(palette, Palette.EventType.AFTER_HIGHLIGHT, handler);
    palette.setHighlightedIndex(2);
    assertEquals(1, handler.getCallCount());
    palette.setHighlightedIndex(-1);
    assertEquals(2, handler.getCallCount());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHighlightItemUpdatesParentA11yActiveDescendant() {
    const container = new Container();
    container.render(document.getElementById('sandbox'));
    container.addChild(palette, true);

    palette.setHighlightedItem(nodes[0]);
    assertEquals(
        'Node 0 cell should be the container\'s active descendant',
        palette.getRenderer().getCellForItem(nodes[0]),
        aria.getActiveDescendant(container.getElement()));

    palette.setHighlightedItem(nodes[1]);
    assertEquals(
        'Node 1 cell should be the container\'s active descendant',
        palette.getRenderer().getCellForItem(nodes[1]),
        aria.getActiveDescendant(container.getElement()));

    palette.setHighlightedItem();
    assertNull(
        'Container should have no active descendant',
        aria.getActiveDescendant(container.getElement()));

    container.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHighlightCellEvents() {
    const container = new Container();
    container.render(document.getElementById('sandbox'));
    container.addChild(palette, true);
    const renderer = palette.getRenderer();

    let events = [];
    let targetElements = [];
    const handleEvent = (e) => {
      events.push(e);
      targetElements.push(e.target.getElement());
    };
    palette.getHandler().listen(
        palette,
        [
          this,
          Component.EventType.HIGHLIGHT,
          this,
          Component.EventType.UNHIGHLIGHT,
        ],
        handleEvent);

    // Test highlight events on first selection
    palette.setHighlightedItem(nodes[0]);
    assertEquals('Should have fired 1 event', 1, events.length);
    assertEquals(
        'HIGHLIGHT event should be fired', Component.EventType.HIGHLIGHT,
        events[0].type);
    assertEquals(
        'Event should be fired for node[0] cell',
        renderer.getCellForItem(nodes[0]), targetElements[0]);

    events = [];
    targetElements = [];

    // Only fire highlight events when there is a selection change
    palette.setHighlightedItem(nodes[0]);
    assertEquals('Should have fired 0 events', 0, events.length);

    // Test highlight events on cell change
    palette.setHighlightedItem(nodes[1]);
    assertEquals('Should have fired 2 events', 2, events.length);
    const unhighlightEvent = events.shift();
    const highlightEvent = events.shift();
    assertEquals(
        'UNHIGHLIGHT should be fired first', Component.EventType.UNHIGHLIGHT,
        unhighlightEvent.type);
    assertEquals(
        'UNHIGHLIGHT should be fired for node[0] cell',
        renderer.getCellForItem(nodes[0]), targetElements[0]);
    assertEquals(
        'HIGHLIGHT should be fired after UNHIGHLIGHT',
        Component.EventType.HIGHLIGHT, highlightEvent.type);
    assertEquals(
        'HIGHLIGHT should be fired for node[1] cell',
        renderer.getCellForItem(nodes[1]), targetElements[1]);

    events = [];
    targetElements = [];

    // Test highlight events when a cell is unselected
    palette.setHighlightedItem();

    assertEquals('Should have fired 1 event', 1, events.length);
    assertEquals(
        'UNHIGHLIGHT event should be fired', Component.EventType.UNHIGHLIGHT,
        events[0].type);
    assertEquals(
        'Event should be fired for node[1] cell',
        renderer.getCellForItem(nodes[1]), targetElements[0]);
  },

  testHandleKeyEventLoops() {
    const container = new Container();
    container.render(document.getElementById('sandbox'));
    container.addChild(palette, true);

    palette.setHighlightedIndex(0);
    const createKeyEvent = (keyCode) => {
      return new KeyEvent(
          keyCode, 0 /* charCode */, false /* repeat */,
          new GoogTestingEvent(EventType.KEYDOWN));
    };
    palette.handleKeyEvent(createKeyEvent(KeyCodes.LEFT));
    assertEquals(nodes.length - 1, palette.getHighlightedIndex());

    palette.handleKeyEvent(createKeyEvent(KeyCodes.RIGHT));
    assertEquals(0, palette.getHighlightedIndex());
  },

  testHandleKeyEventScrollIntoView() {
    // Set the palette to have 5 columns. Since the palette has 23 items, it
    // will have 5 rows (with last row containing only 3 items).
    palette.setSize(5 /* number of columns */);

    const container = new Container();
    container.render(document.getElementById('sandbox'));
    container.addChild(palette, true);
    const containerEl = container.getElementStrict();
    // Set container height to be smaller than content height and add scrolling.
    style.setSize(containerEl, 400, 50);
    style.setStyle(containerEl, 'overflow', 'auto');

    // Pressing down arrow key 4 times should move highlight from index 0 to
    // index 20 (first item of the last row). Verify that this causes the
    // container to scroll.
    const item20 = palette.getRenderer().getCellForItem(nodes[20]);
    const pressDownArrowKeyFourTimes = () => {
      const downArrayKeyEvent = new KeyEvent(
          KeyCodes.DOWN, 0 /* charCode */, false /* repeat */,
          new GoogTestingEvent(EventType.KEYDOWN));
      for (let i = 0; i < 4; i++) {
        palette.handleKeyEvent(downArrayKeyEvent);
      }
    };

    palette.setHighlightedIndex(0);
    assert(style.getContainerOffsetToScrollInto(item20, containerEl).y > 0);
    assertEquals(0, containerEl.scrollTop);
    pressDownArrowKeyFourTimes();
    assertEquals(20, palette.getHighlightedIndex());
    assert(
        'Container should scroll down to make the highlighted item visible.',
        containerEl.scrollTop > 0);
  },

  testSetHighlight() {
    assertEquals(-1, palette.getHighlightedIndex());
    palette.setHighlighted(true);
    assertEquals(0, palette.getHighlightedIndex());

    palette.setHighlightedIndex(3);
    palette.setHighlighted(false);
    assertEquals(-1, palette.getHighlightedIndex());
    palette.setHighlighted(true);
    assertEquals(3, palette.getHighlightedIndex());

    palette.setHighlighted(false);
    palette.setHighlightedIndex(5);
    palette.setHighlighted(true);
    assertEquals(5, palette.getHighlightedIndex());
    palette.setHighlighted(true);
    assertEquals(5, palette.getHighlightedIndex());
  },

  testPerformActionInternal() {
    const container = new Container();
    container.render(document.getElementById('sandbox'));
    container.addChild(palette, true);
    palette.setActive(true);
    palette.setSelectedIndex(1);
    palette.setHighlightedIndex(3);
    palette.setHighlighted(true);
    assertEquals(1, palette.getSelectedIndex());
    assertEquals(3, palette.getHighlightedIndex());

    // Click somewhere in the palette, but not inside a cell.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const mouseUp = new googEvents.BrowserEvent(
        {type: 'mouseup', button: 1, target: palette});
    palette.handleMouseUp(mouseUp);

    // Highlight and selection are both unchanged (user did not select
    // anything).
    assertEquals(1, palette.getSelectedIndex());
    assertEquals(3, palette.getHighlightedIndex());
  },

  testSetAriaLabel() {
    assertNull(
        'Palette must not have aria label by default', palette.getAriaLabel());
    palette.setAriaLabel('My Palette');
    palette.render();
    const element = palette.getElementStrict();
    assertNotNull('Element must not be null', element);
    assertEquals(
        'Palette element must have expected aria-label', 'My Palette',
        element.getAttribute('aria-label'));
    palette.setAriaLabel('My new Palette');
    assertEquals(
        'Palette element must have updated aria-label', 'My new Palette',
        element.getAttribute('aria-label'));
  },
});

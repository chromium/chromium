/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.SplitPaneTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Size = goog.require('goog.math.Size');
const SplitPane = goog.require('goog.ui.SplitPane');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let splitpane;
let leftComponent;
let rightComponent;

testSuite({
  setUp() {
    leftComponent = new Component();
    rightComponent = new Component();
    splitpane = new SplitPane(
        leftComponent, rightComponent, SplitPane.Orientation.HORIZONTAL);
  },

  tearDown() {
    splitpane.dispose();
    leftComponent.dispose();
    rightComponent.dispose();
    dom.removeChildren(dom.getElement('sandbox'));
  },

  testRender() {
    splitpane.render(dom.getElement('sandbox'));
    assertEquals(
        1,
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-splitpane').length);
    assertEquals(
        1,
        dom.getElementsByTagNameAndClass(
               TagName.DIV, 'goog-splitpane-first-container')
            .length);
    assertEquals(
        1,
        dom.getElementsByTagNameAndClass(
               TagName.DIV, 'goog-splitpane-second-container')
            .length);
    assertEquals(
        1,
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-splitpane-handle')
            .length);
  },

  testDecorate() {
    const mainDiv = dom.createDom(
        TagName.DIV, 'goog-splitpane',
        dom.createDom(TagName.DIV, 'goog-splitpane-first-container'),
        dom.createDom(TagName.DIV, 'goog-splitpane-second-container'),
        dom.createDom(TagName.DIV, 'goog-splitpane-handle'));
    const sandbox = dom.getElement('sandbox');
    dom.appendChild(sandbox, mainDiv);

    splitpane.decorate(mainDiv);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDecorateWithNestedSplitPane() {
    // Create a standard split pane to be nested within another split pane.
    const innerSplitPaneDiv = dom.createDom(
        TagName.DIV, 'goog-splitpane',
        dom.createDom(TagName.DIV, 'goog-splitpane-first-container e1'),
        dom.createDom(TagName.DIV, 'goog-splitpane-second-container e2'),
        dom.createDom(TagName.DIV, 'goog-splitpane-handle e3'));

    // Create a split pane containing a split pane instance.
    const outerSplitPaneDiv = dom.createDom(
        TagName.DIV, 'goog-splitpane',
        dom.createDom(
            TagName.DIV, 'goog-splitpane-first-container e4',
            innerSplitPaneDiv),
        dom.createDom(TagName.DIV, 'goog-splitpane-second-container e5'),
        dom.createDom(TagName.DIV, 'goog-splitpane-handle e6'));

    const sandbox = dom.getElement('sandbox');
    dom.appendChild(sandbox, outerSplitPaneDiv);

    // Decorate and check that the correct containers and handle are used.
    splitpane.decorate(outerSplitPaneDiv);
    assertTrue(classlist.contains(splitpane.firstComponentContainer_, 'e4'));
    assertTrue(classlist.contains(splitpane.secondComponentContainer_, 'e5'));
    assertTrue(classlist.contains(splitpane.splitpaneHandle_, 'e6'));
  },

  testSetSize() {
    splitpane.setInitialSize(200);
    splitpane.setHandleSize(10);
    splitpane.render(dom.getElement('sandbox'));

    splitpane.setSize(new Size(500, 300));
    assertEquals(200, splitpane.getFirstComponentSize());

    const splitpaneSize = style.getBorderBoxSize(splitpane.getElement());
    assertEquals(500, splitpaneSize.width);
    assertEquals(300, splitpaneSize.height);
  },

  testOrientationChange() {
    splitpane.setInitialSize(200);
    splitpane.setHandleSize(10);
    splitpane.render(dom.getElement('sandbox'));
    splitpane.setSize(new Size(500, 300));

    const first = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-first-container')[0];
    const second = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-second-container')[0];
    const handle = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-handle')[0];

    let handleSize = style.getBorderBoxSize(handle);
    assertEquals(10, handleSize.width);
    assertEquals(300, handleSize.height);

    let firstSize = style.getBorderBoxSize(first);
    assertEquals(200, firstSize.width);
    assertEquals(300, firstSize.height);

    let secondSize = style.getBorderBoxSize(second);
    assertEquals(290, secondSize.width);  // 500 - 200 - 10 = 290
    assertEquals(300, secondSize.height);

    splitpane.setOrientation(SplitPane.Orientation.VERTICAL);

    handleSize = style.getBorderBoxSize(handle);
    assertEquals(10, handleSize.height);
    assertEquals(500, handleSize.width);

    firstSize = style.getBorderBoxSize(first);
    assertEquals(120, firstSize.height);  // 200 * 300/500 = 120
    assertEquals(500, firstSize.width);

    secondSize = style.getBorderBoxSize(second);
    assertEquals(170, secondSize.height);  // 300 - 120 - 10 = 170
    assertEquals(500, secondSize.width);

    splitpane.setOrientation(SplitPane.Orientation.HORIZONTAL);

    handleSize = style.getBorderBoxSize(handle);
    assertEquals(10, handleSize.width);
    assertEquals(300, handleSize.height);

    firstSize = style.getBorderBoxSize(first);
    assertEquals(200, firstSize.width);
    assertEquals(300, firstSize.height);

    secondSize = style.getBorderBoxSize(second);
    assertEquals(290, secondSize.width);
    assertEquals(300, secondSize.height);
  },

  testDragEvent() {
    splitpane.setInitialSize(200);
    splitpane.setHandleSize(10);
    splitpane.render(dom.getElement('sandbox'));

    const handler = recordFunction();
    events.listen(splitpane, SplitPane.EventType.HANDLE_DRAG, handler);
    const handle = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-handle')[0];

    testingEvents.fireMouseDownEvent(handle);
    testingEvents.fireMouseMoveEvent(handle);
    testingEvents.fireMouseUpEvent(handle);
    assertEquals('HANDLE_DRAG event expected', 1, handler.getCallCount());

    splitpane.setContinuousResize(false);
    handler.reset();
    testingEvents.fireMouseDownEvent(handle);
    testingEvents.fireMouseMoveEvent(handle);
    testingEvents.fireMouseUpEvent(handle);
    assertEquals('HANDLE_DRAG event not expected', 0, handler.getCallCount());
  },

  testDragEndEvent() {
    splitpane.setInitialSize(200);
    splitpane.setHandleSize(10);
    splitpane.render(dom.getElement('sandbox'));
    const handler = recordFunction();
    events.listen(splitpane, SplitPane.EventType.HANDLE_DRAG_END, handler);

    const handle = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-handle')[0];

    testingEvents.fireMouseDownEvent(handle);
    testingEvents.fireMouseMoveEvent(handle);
    testingEvents.fireMouseUpEvent(handle);
    assertEquals('HANDLE_DRAG_END event expected', 1, handler.getCallCount());

    splitpane.setContinuousResize(false);
    handler.reset();
    testingEvents.fireMouseDownEvent(handle);
    testingEvents.fireMouseMoveEvent(handle);
    testingEvents.fireMouseUpEvent(handle);
    assertEquals('HANDLE_DRAG_END event expected', 1, handler.getCallCount());
  },

  testSnapEvent() {
    splitpane.setInitialSize(200);
    splitpane.setHandleSize(10);
    splitpane.render(dom.getElement('sandbox'));
    const handler = recordFunction();
    events.listen(splitpane, SplitPane.EventType.HANDLE_SNAP, handler);
    const handle = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'goog-splitpane-handle')[0];
    testingEvents.fireDoubleClickSequence(handle);
    assertEquals('HANDLE_SNAP event expected', 1, handler.getCallCount());
  },
});

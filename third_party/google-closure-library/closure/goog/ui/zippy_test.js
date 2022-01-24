/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ZippyTest');
goog.setTestOnly();

const KeyCodes = goog.require('goog.events.KeyCodes');
const TagName = goog.require('goog.dom.TagName');
const Zippy = goog.require('goog.ui.Zippy');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let buttonZippy;
let contentlessZippy;
let fakeZippy1;
let fakeZippy2;
let headerlessZippy;
let zippy;

let lazyZippy;
let lazyZippyCallCount;
let lazyZippyContentEl;
let dualHeaderZippy;
let dualHeaderZippyCollapsedHeaderEl;
let dualHeaderZippyExpandedHeaderEl;

function hasCollapseOrExpandClasses(el) {
  const isCollapsed = classlist.contains(el, 'goog-zippy-collapsed');
  const isExpanded = classlist.contains(el, 'goog-zippy-expanded');
  return isCollapsed || isExpanded;
}

testSuite({
  setUp() {
    zippy = new Zippy(dom.getElement('t1'), dom.getElement('c1'));

    const fakeControlEl = dom.createElement(TagName.BUTTON);
    const fakeContentEl = dom.createElement(TagName.DIV);

    fakeZippy1 = new Zippy(
        fakeControlEl.cloneNode(true), fakeContentEl.cloneNode(true), true);
    fakeZippy2 = new Zippy(
        fakeControlEl.cloneNode(true), fakeContentEl.cloneNode(true), false);
    contentlessZippy =
        new Zippy(fakeControlEl.cloneNode(true), undefined, true);
    headerlessZippy = new Zippy(null, fakeContentEl.cloneNode(true), true);
    buttonZippy = new Zippy(
        null, fakeContentEl.cloneNode(true), true, null, null,
        aria.Role.BUTTON);

    lazyZippyCallCount = 0;
    lazyZippyContentEl = fakeContentEl.cloneNode(true);
    lazyZippy = new Zippy(dom.getElement('t1'), () => {
      lazyZippyCallCount++;
      return lazyZippyContentEl;
    });
    dualHeaderZippyCollapsedHeaderEl = fakeControlEl.cloneNode(true);
    dualHeaderZippyExpandedHeaderEl = fakeControlEl.cloneNode(true);
    dualHeaderZippy = new Zippy(
        dualHeaderZippyCollapsedHeaderEl, fakeContentEl.cloneNode(true), false,
        dualHeaderZippyExpandedHeaderEl);
  },

  testConstructor() {
    assertNotNull('must not be null', zippy);
  },

  testIsExpanded() {
    assertEquals('Default expanded must be false', false, zippy.isExpanded());
    assertEquals('Expanded must be true', true, fakeZippy1.isExpanded());
    assertEquals('Expanded must be false', false, fakeZippy2.isExpanded());
    assertEquals('Expanded must be true', true, headerlessZippy.isExpanded());
    assertEquals('Expanded must be false', false, lazyZippy.isExpanded());
    assertEquals('Expanded must be false', false, dualHeaderZippy.isExpanded());
    assertEquals('Expanded must be true', true, buttonZippy.isExpanded());
  },

  tearDown() {
    zippy.dispose();
    fakeZippy1.dispose();
    fakeZippy2.dispose();
    contentlessZippy.dispose();
    headerlessZippy.dispose();
    lazyZippy.dispose();
    dualHeaderZippy.dispose();
    buttonZippy.dispose();
  },

  testExpandCollapse() {
    zippy.expand();
    headerlessZippy.expand();
    assertEquals('expanded must be true', true, zippy.isExpanded());
    assertEquals('expanded must be true', true, headerlessZippy.isExpanded());

    zippy.collapse();
    headerlessZippy.collapse();
    assertEquals('expanded must be false', false, zippy.isExpanded());
    assertEquals('expanded must be false', false, headerlessZippy.isExpanded());
  },

  testExpandCollapse_lazyZippy() {
    assertEquals('callback should not be called #1.', 0, lazyZippyCallCount);
    lazyZippy.collapse();
    assertEquals('callback should not be called #2.', 0, lazyZippyCallCount);

    lazyZippy.expand();
    assertEquals('callback should be called once #1.', 1, lazyZippyCallCount);
    assertEquals('expanded must be true', true, lazyZippy.isExpanded());
    assertEquals(
        'contentEl should be visible', '', lazyZippyContentEl.style.display);

    lazyZippy.collapse();
    assertEquals('callback should be called once #2.', 1, lazyZippyCallCount);
    assertEquals('expanded must be false', false, lazyZippy.isExpanded());
    assertEquals(
        'contentEl should not be visible', 'none',
        lazyZippyContentEl.style.display);

    lazyZippy.expand();
    assertEquals('callback should be called once #3.', 1, lazyZippyCallCount);
    assertEquals('expanded must be true #2', true, lazyZippy.isExpanded());
    assertEquals(
        'contentEl should be visible #2', '', lazyZippyContentEl.style.display);
  },

  testExpandCollapse_dualHeaderZippy() {
    dualHeaderZippy.expand();
    assertEquals('expanded must be true', true, dualHeaderZippy.isExpanded());
    assertFalse(
        'collapsed header should not have state class name #1',
        hasCollapseOrExpandClasses(dualHeaderZippyCollapsedHeaderEl));
    assertFalse(
        'expanded header should not have state class name #1',
        hasCollapseOrExpandClasses(dualHeaderZippyExpandedHeaderEl));

    dualHeaderZippy.collapse();
    assertEquals('expanded must be false', false, dualHeaderZippy.isExpanded());
    assertFalse(
        'collapsed header should not have state class name #2',
        hasCollapseOrExpandClasses(dualHeaderZippyCollapsedHeaderEl));
    assertFalse(
        'expanded header should not have state class name #2',
        hasCollapseOrExpandClasses(dualHeaderZippyExpandedHeaderEl));
  },

  testSetExpand() {
    const expanded = !zippy.isExpanded();
    zippy.setExpanded(expanded);
    assertEquals(`expanded must be ${expanded}`, expanded, zippy.isExpanded());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCssClassesAndAria() {
    assertTrue(
        'goog-zippy-header is enabled',
        classlist.contains(zippy.elHeader_, 'goog-zippy-header'));
    assertNotNull(zippy.elHeader_);
    assertEquals(
        'header aria-expanded is false', 'false',
        aria.getState(zippy.elHeader_, 'expanded'));
    zippy.setExpanded(true);
    assertTrue(
        'goog-zippy-content is enabled',
        classlist.contains(zippy.getContentElement(), 'goog-zippy-content'));
    assertEquals(
        'header aria role is TAB', 'tab', aria.getRole(zippy.elHeader_));
    assertEquals(
        'header aria-expanded is true', 'true',
        aria.getState(zippy.elHeader_, 'expanded'));
  },

  testAreaRoleOverride() {
    assertEquals(
        'Aria role override is goog.a11y.aria.Role.BUTTON', aria.Role.BUTTON,
        buttonZippy.getAriaRole());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHeaderTabIndex() {
    assertEquals('Header tabIndex is 0', 0, zippy.elHeader_.tabIndex);
  },

  testGetVisibleHeaderElement() {
    dualHeaderZippy.setExpanded(false);
    assertEquals(
        dualHeaderZippyCollapsedHeaderEl,
        dualHeaderZippy.getVisibleHeaderElement());
    dualHeaderZippy.setExpanded(true);
    assertEquals(
        dualHeaderZippyExpandedHeaderEl,
        dualHeaderZippy.getVisibleHeaderElement());
  },

  testToggle() {
    const expanded = !zippy.isExpanded();
    zippy.toggle();
    assertEquals(`expanded must be ${expanded}`, expanded, zippy.isExpanded());
  },

  testCustomEventTOGGLE() {
    let dispatchedActionCount;
    const handleAction = () => {
      dispatchedActionCount++;
    };

    const doTest = (zippyObj) => {
      dispatchedActionCount = 0;
      events.listen(zippyObj, Zippy.Events.TOGGLE, handleAction);
      zippy.toggle();
      assertEquals('Custom Event must be called ', 1, dispatchedActionCount);
    };

    doTest(zippy);
    doTest(fakeZippy1);
    doTest(contentlessZippy);
    doTest(headerlessZippy);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testActionEvent() {
    let actionEventCount = 0;
    let toggleEventCount = 0;
    const handleEvent = (e) => {
      if (e.type == Zippy.Events.TOGGLE) {
        toggleEventCount++;
      } else if (e.type == Zippy.Events.ACTION) {
        actionEventCount++;
        assertNotNull(
            'action event must have triggering event', e.triggeringEvent);
        assertTrue(
            'toggle must have been called first',
            toggleEventCount >= actionEventCount);
      }
    };
    events.listen(zippy, googObject.getValues(Zippy.Events), handleEvent);
    testingEvents.fireClickSequence(zippy.elHeader_);
    assertEquals('Zippy ACTION event fired', 1, actionEventCount);
    assertEquals('Zippy TOGGLE event fired', 1, toggleEventCount);

    zippy.toggle();
    assertEquals('Zippy ACTION event NOT fired', 1, actionEventCount);
    assertEquals('Zippy TOGGLE event fired', 2, toggleEventCount);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testBasicZippyBehavior() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };

    events.listen(zippy, Zippy.Events.TOGGLE, handleAction);
    testingEvents.fireClickSequence(zippy.elHeader_);
    assertEquals(
        'Zippy  must have dispatched TOGGLE on click', 1,
        dispatchedActionCount);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsHandleKeyEvent() {
    zippy.setHandleKeyboardEvents(false);
    assertFalse('Zippy is not handling key events', zippy.isHandleKeyEvents());
    assertTrue(
        'Zippy setHandleKeyEvents does not affect handling mouse events',
        zippy.isHandleMouseEvents());
    assertEquals(0, zippy.keyboardEventHandler_.getListenerCount());

    zippy.setHandleKeyboardEvents(true);
    assertTrue('Zippy is handling key events', zippy.isHandleKeyEvents());
    assertTrue(
        'Zippy setHandleKeyEvents does not affect handling mouse events',
        zippy.isHandleMouseEvents());
    assertNotEquals(0, zippy.keyboardEventHandler_.getListenerCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsHandleMouseEvent() {
    zippy.setHandleMouseEvents(false);
    assertFalse(
        'Zippy is not handling mouse events', zippy.isHandleMouseEvents());
    assertTrue(
        'Zippy setHandleMouseEvents does not affect handling key events',
        zippy.isHandleKeyEvents());
    assertEquals(0, zippy.mouseEventHandler_.getListenerCount());

    zippy.setHandleMouseEvents(true);
    assertTrue('Zippy is handling mouse events', zippy.isHandleMouseEvents());
    assertTrue(
        'Zippy setHandleMouseEvents does not affect handling key events',
        zippy.isHandleKeyEvents());
    assertNotEquals(0, zippy.mouseEventHandler_.getListenerCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyDownEventTriggersHeader() {
    let actionEventCount = 0;
    let toggleEventCount = 0;
    const handleEvent = (e) => {
      if (e.type == Zippy.Events.TOGGLE) {
        toggleEventCount++;
      } else if (e.type == Zippy.Events.ACTION) {
        actionEventCount++;
        assertTrue(
            'toggle must have been called first',
            toggleEventCount >= actionEventCount);
      }
    };
    zippy.setHandleKeyboardEvents(true);
    events.listen(zippy, googObject.getValues(Zippy.Events), handleEvent);

    testingEvents.fireKeySequence(zippy.elHeader_, KeyCodes.SPACE);

    assertEquals('Zippy ACTION event fired', 1, actionEventCount);
    assertEquals('Zippy TOGGLE event fired', 1, toggleEventCount);
  },
});

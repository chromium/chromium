/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ModalPopupTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const MockClock = goog.require('goog.testing.MockClock');
const ModalPopup = goog.require('goog.ui.ModalPopup');
const PopupBase = goog.require('goog.ui.PopupBase');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const Transition = goog.require('goog.fx.Transition');
const aria = goog.require('goog.a11y.aria');
const css3 = goog.require('goog.fx.css3');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googString = goog.require('goog.string');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let popup;
let main;
let mockClock;

/** @implements {Transition} */
class MockTransition extends GoogEventTarget {
  constructor() {
    super();
    this.wasPlayed = false;
  }

  play() {
    this.wasPlayed = true;
  }

  stop() {}
}

testSuite({
  setUp() {
    main = /** @type {!Element} */ (dom.getElement('main'));
    mockClock = new MockClock(true);
  },

  tearDown() {
    dispose(popup);
    mockClock.dispose();
    aria.removeState(main, State.HIDDEN);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testOrientationChange() {
    let i = 0;
    popup = new ModalPopup();
    /** @suppress {visibility} suppression added to enable type checking */
    popup.resizeBackgroundTask_ = () => {
      i++;
    };
    popup.render();
    popup.setVisible(true);
    const event = new events.Event(
        EventType.ORIENTATIONCHANGE, popup.getDomHelper().getWindow());

    testingEvents.fireBrowserEvent(event);
    assertEquals(1, i);

    testingEvents.fireBrowserEvent(event);
    assertEquals(2, i);

    popup.setVisible(false);
    testingEvents.fireBrowserEvent(event);
    assertEquals(2, i);
  },

  testDispose() {
    popup = new ModalPopup();
    popup.render();

    dispose(popup);
    assertNull(dom.getElementByClass('goog-modalpopup-bg'));
    assertNull(dom.getElementByClass('goog-modalpopup'));
    assertEquals(0, dom.getElementsByTagNameAndClass(TagName.SPAN).length);
  },

  testRenderWithoutIframeMask() {
    popup = new ModalPopup();
    popup.render();

    assertEquals(
        0,
        dom.getElementsByTagNameAndClass(TagName.IFRAME, 'goog-modalpopup-bg')
            .length);

    const bg =
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-modalpopup-bg');
    assertEquals(1, bg.length);
    const content = dom.getElementByClass('goog-modalpopup');
    assertNotNull(content);
    const tabCatcher = dom.getElementsByTagNameAndClass(TagName.SPAN);
    assertEquals(1, tabCatcher.length);

    assertTrue(dom.compareNodeOrder(bg[0], content) < 0);
    assertTrue(dom.compareNodeOrder(content, tabCatcher[0]) < 0);
    assertTrue(googString.isEmptyOrWhitespace(
        googString.makeSafe(aria.getState(main, State.HIDDEN))));
    popup.setVisible(true);
    assertTrue(googString.isEmptyOrWhitespace(googString.makeSafe(
        aria.getState(popup.getElementStrict(), State.HIDDEN))));
    assertEquals('true', aria.getState(main, State.HIDDEN));
    popup.setVisible(false);
    assertTrue(googString.isEmptyOrWhitespace(
        googString.makeSafe(aria.getState(main, State.HIDDEN))));
  },

  testRenderWithIframeMask() {
    popup = new ModalPopup(true);
    popup.render();

    const iframe =
        dom.getElementsByTagNameAndClass(TagName.IFRAME, 'goog-modalpopup-bg');
    assertEquals(1, iframe.length);
    const bg =
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-modalpopup-bg');
    assertEquals(1, bg.length);
    const content = dom.getElementByClass('goog-modalpopup');
    assertNotNull(content);
    const tabCatcher = dom.getElementsByTagNameAndClass(TagName.SPAN);
    assertEquals(1, tabCatcher.length);

    assertTrue(dom.compareNodeOrder(iframe[0], bg[0]) < 0);
    assertTrue(dom.compareNodeOrder(bg[0], content) < 0);
    assertTrue(dom.compareNodeOrder(content, tabCatcher[0]) < 0);
    assertTrue(googString.isEmptyOrWhitespace(
        googString.makeSafe(aria.getState(main, State.HIDDEN))));
    popup.setVisible(true);
    assertTrue(googString.isEmptyOrWhitespace(googString.makeSafe(
        aria.getState(popup.getElementStrict(), State.HIDDEN))));
    assertEquals('true', aria.getState(main, State.HIDDEN));
    popup.setVisible(false);
    assertTrue(googString.isEmptyOrWhitespace(
        googString.makeSafe(aria.getState(main, State.HIDDEN))));
  },

  testRenderWithAriaState() {
    popup = new ModalPopup();
    popup.render();

    aria.setState(main, State.HIDDEN, true);
    popup.setVisible(true);
    assertEquals('true', aria.getState(main, State.HIDDEN));
    popup.setVisible(false);
    assertEquals('true', aria.getState(main, State.HIDDEN));

    aria.setState(main, State.HIDDEN, false);
    popup.setVisible(true);
    assertEquals('false', aria.getState(main, State.HIDDEN));
    popup.setVisible(false);
    assertEquals('false', aria.getState(main, State.HIDDEN));
  },

  testRenderDoesNotShowAnyElement() {
    popup = new ModalPopup(true);
    popup.render();

    const iframe =
        dom.getElementsByTagNameAndClass(TagName.IFRAME, 'goog-modalpopup-bg');
    assertFalse(style.isElementShown(iframe[0]));
    const bg =
        dom.getElementsByTagNameAndClass(TagName.DIV, 'goog-modalpopup-bg');
    assertFalse(style.isElementShown(bg[0]));
    assertFalse(style.isElementShown(dom.getElementByClass('goog-modalpopup')));
    const tabCatcher = dom.getElementsByTagNameAndClass(TagName.SPAN);
    assertFalse(style.isElementShown(tabCatcher[0]));
  },

  testIframeOpacityIsSetToZero() {
    popup = new ModalPopup(true);
    popup.render();

    const iframe = dom.getElementsByTagNameAndClass(
        TagName.IFRAME, 'goog-modalpopup-bg')[0];
    assertEquals(0, style.getOpacity(iframe));
  },

  testEventFiredOnShow() {
    popup = new ModalPopup(true);
    popup.render();

    let beforeShowCallCount = 0;
    const beforeShowHandler = () => {
      beforeShowCallCount++;
    };
    let showCallCount = false;
    /**
     * @suppress {strictPrimitiveOperators} suppression added to enable type
     * checking
     */
    const showHandler = () => {
      assertEquals(
          'BEFORE_SHOW is not dispatched before SHOW', 1, beforeShowCallCount);
      showCallCount++;
    };

    events.listen(popup, PopupBase.EventType.BEFORE_SHOW, beforeShowHandler);
    events.listen(popup, PopupBase.EventType.SHOW, showHandler);

    popup.setVisible(true);

    assertEquals(1, beforeShowCallCount);
    assertEquals(1, showCallCount);
  },

  testEventFiredOnHide() {
    popup = new ModalPopup(true);
    popup.render();
    popup.setVisible(true);

    let beforeHideCallCount = 0;
    const beforeHideHandler = () => {
      beforeHideCallCount++;
    };
    let hideCallCount = false;
    /**
     * @suppress {strictPrimitiveOperators} suppression added to enable type
     * checking
     */
    const hideHandler = () => {
      assertEquals(
          'BEFORE_HIDE is not dispatched before HIDE', 1, beforeHideCallCount);
      hideCallCount++;
    };

    events.listen(popup, PopupBase.EventType.BEFORE_HIDE, beforeHideHandler);
    events.listen(popup, PopupBase.EventType.HIDE, hideHandler);

    popup.setVisible(false);

    assertEquals(1, beforeHideCallCount);
    assertEquals(1, hideCallCount);
  },

  testShowEventFiredWithNoTransition() {
    popup = new ModalPopup();
    popup.render();

    let showHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.SHOW, () => {
      showHandlerCalled = true;
    });

    popup.setVisible(true);
    assertTrue(showHandlerCalled);
  },

  testHideEventFiredWithNoTransition() {
    popup = new ModalPopup();
    popup.render();

    let hideHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.HIDE, () => {
      hideHandlerCalled = true;
    });

    popup.setVisible(true);
    popup.setVisible(false);
    assertTrue(hideHandlerCalled);
  },

  testTransitionsPlayedOnShow() {
    popup = new ModalPopup();
    popup.render();

    const mockPopupShowTransition = new MockTransition();
    const mockPopupHideTransition = new MockTransition();
    const mockBgShowTransition = new MockTransition();
    const mockBgHideTransition = new MockTransition();

    let showHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.SHOW, () => {
      showHandlerCalled = true;
    });

    popup.setTransition(
        mockPopupShowTransition, mockPopupHideTransition, mockBgShowTransition,
        mockBgHideTransition);
    assertFalse(mockPopupShowTransition.wasPlayed);
    assertFalse(mockBgShowTransition.wasPlayed);

    popup.setVisible(true);
    assertTrue(mockPopupShowTransition.wasPlayed);
    assertTrue(mockBgShowTransition.wasPlayed);

    assertFalse(showHandlerCalled);
    mockPopupShowTransition.dispatchEvent(Transition.EventType.END);
    assertTrue(showHandlerCalled);
  },

  testTransitionsPlayedOnHide() {
    popup = new ModalPopup();
    popup.render();

    const mockPopupShowTransition = new MockTransition();
    const mockPopupHideTransition = new MockTransition();
    const mockBgShowTransition = new MockTransition();
    const mockBgHideTransition = new MockTransition();

    let hideHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.HIDE, () => {
      hideHandlerCalled = true;
    });

    popup.setTransition(
        mockPopupShowTransition, mockPopupHideTransition, mockBgShowTransition,
        mockBgHideTransition);
    popup.setVisible(true);
    assertFalse(mockPopupHideTransition.wasPlayed);
    assertFalse(mockBgHideTransition.wasPlayed);

    popup.setVisible(false);
    assertTrue(mockPopupHideTransition.wasPlayed);
    assertTrue(mockBgHideTransition.wasPlayed);

    assertFalse(hideHandlerCalled);
    mockPopupHideTransition.dispatchEvent(Transition.EventType.END);
    assertTrue(hideHandlerCalled);
  },

  testTransitionsAndDisposingOnHideWorks() {
    popup = new ModalPopup();
    popup.render();

    events.listen(popup, PopupBase.EventType.HIDE, () => {
      popup.dispose();
    });

    const popupShowTransition =
        css3.fadeIn(popup.getElement(), 0.1 /* duration */);
    const popupHideTransition =
        css3.fadeOut(popup.getElement(), 0.1 /* duration */);
    const bgShowTransition =
        css3.fadeIn(popup.getElement(), 0.1 /* duration */);
    const bgHideTransition =
        css3.fadeOut(popup.getElement(), 0.1 /* duration */);

    popup.setTransition(
        popupShowTransition, popupHideTransition, bgShowTransition,
        bgHideTransition);
    popup.setVisible(true);
    popup.setVisible(false);
    // Nothing to assert. We only want to ensure that there is no error.
  },

  testSetVisibleWorksCorrectlyWithTransitions() {
    popup = new ModalPopup();
    popup.render();
    popup.setTransition(
        css3.fadeIn(popup.getElement(), 1),
        css3.fadeIn(popup.getBackgroundElement(), 1),
        css3.fadeOut(popup.getElement(), 1),
        css3.fadeOut(popup.getBackgroundElement(), 1));

    // Consecutive calls to setVisible works without needing to wait for
    // transition to finish.
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    popup.setVisible(false);
    assertFalse(popup.isVisible());
    mockClock.tick(1100);

    // Calling setVisible(true) immediately changed the state to visible.
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    mockClock.tick(1100);

    // Consecutive calls to setVisible, in opposite order.
    popup.setVisible(false);
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    mockClock.tick(1100);

    // Calling setVisible(false) immediately changed the state to not visible.
    popup.setVisible(false);
    assertFalse(popup.isVisible());
    mockClock.tick(1100);
  },

  testTransitionsDisposed() {
    popup = new ModalPopup();
    popup.render();

    const transition = css3.fadeIn(popup.getElement(), 0.1 /* duration */);

    let hideHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.HIDE, () => {
      hideHandlerCalled = true;
    });

    popup.setTransition(transition, transition, transition, transition);
    popup.dispose();

    transition.dispatchEvent(Transition.EventType.END);
    assertFalse(hideHandlerCalled);
  },

  testBackgroundHeight() {
    // Insert an absolutely-positioned element larger than the viewport.
    const viewportSize = dom.getViewportSize();
    const w = viewportSize.width * 2;
    const h = viewportSize.height * 2;
    const dummy = dom.createElement(TagName.DIV);
    dummy.style.position = 'absolute';
    style.setSize(dummy, w, h);
    document.body.appendChild(dummy);

    try {
      popup = new ModalPopup();
      popup.render();
      popup.setVisible(true);

      const size = style.getSize(popup.getBackgroundElement());
      assertTrue(
          'Background element must cover the size of the content',
          size.width >= w && size.height >= h);
    } finally {
      dom.removeNode(dummy);
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetupBackwardTabWrapResetsFlagAfterTimeout() {
    popup.setupBackwardTabWrap();
    assertTrue(
        'Backward tab wrap should be in progress',
        popup.backwardTabWrapInProgress_);
    mockClock.tick(1);
    assertFalse(
        'Backward tab wrap flag should be reset after delay',
        popup.backwardTabWrapInProgress_);
  },

  testPopupGetsFocus() {
    popup = new ModalPopup();
    popup.render();
    popup.setVisible(true);
    assertTrue(
        'Dialog must receive initial focus',
        dom.getActiveElement(document) == popup.getElement());
  },

  testDecoratedPopupGetsFocus() {
    const dialogElem = dom.createElement(TagName.DIV);
    document.body.appendChild(dialogElem);
    popup = new ModalPopup();
    popup.decorate(dialogElem);
    popup.setVisible(true);
    assertTrue(
        'Dialog must receive initial focus',
        dom.getActiveElement(document) == popup.getElement());
    dom.removeNode(dialogElem);
  },
});

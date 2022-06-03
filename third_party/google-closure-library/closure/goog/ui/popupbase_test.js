/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PopupBaseTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const PopupBase = goog.require('goog.ui.PopupBase');
const TagName = goog.require('goog.dom.TagName');
const Transition = goog.require('goog.fx.Transition');
const css3 = goog.require('goog.fx.css3');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let targetDiv;
let popupDiv;
let partnerDiv;
let clock;
let popup;

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

// TODO(gboyer): Write better unit tests for click and cross-iframe dismissal.
testSuite({
  setUpPage() {
    targetDiv = dom.getElement('targetDiv');
    popupDiv = dom.getElement('popupDiv');
    partnerDiv = dom.getElement('partnerDiv');
  },

  setUp() {
    popup = new PopupBase(popupDiv);
    clock = new MockClock(true);
  },

  tearDown() {
    popup.dispose();
    clock.uninstall();
    document.body.setAttribute('dir', 'ltr');
  },

  testSetVisible() {
    popup.setVisible(true);
    assertEquals('visible', popupDiv.style.visibility);
    assertEquals('', popupDiv.style.display);
    popup.setVisible(false);
    assertEquals('hidden', popupDiv.style.visibility);
    assertEquals('none', popupDiv.style.display);
  },

  testEscapeDismissal() {
    popup.setHideOnEscape(true);
    assertTrue(
        'Sanity check that getHideOnEscape is true when set to true.',
        popup.getHideOnEscape());
    popup.setVisible(true);
    assertFalse(
        'Escape key should be cancelled',
        testingEvents.fireKeySequence(targetDiv, KeyCodes.ESC));
    assertFalse(popup.isVisible());
  },

  testEscapeDismissalCanBeDisabled() {
    popup.setHideOnEscape(false);
    popup.setVisible(true);
    assertTrue(
        'Escape key should be cancelled',
        testingEvents.fireKeySequence(targetDiv, KeyCodes.ESC));
    assertTrue(popup.isVisible());
  },

  testEscapeDismissalIsDisabledByDefault() {
    assertFalse(popup.getHideOnEscape());
  },

  testEscapeDismissalDoesNotRecognizeOtherKeys() {
    popup.setHideOnEscape(true);
    popup.setVisible(true);
    let eventsPropagated = 0;
    events.listenOnce(
        dom.getElement('commonAncestor'),
        [
          EventType.KEYDOWN,
          EventType.KEYUP,
          EventType.KEYPRESS,
        ],
        () => {
          ++eventsPropagated;
        });
    assertTrue('Popup should remain visible', popup.isVisible());
    assertTrue(
        'The key event default action should not be prevented',
        testingEvents.fireKeySequence(targetDiv, KeyCodes.A));
    assertEquals(
        'Keydown, keyup, and keypress should have all propagated', 3,
        eventsPropagated);
  },

  testEscapeDismissalCanBeCancelledByBeforeHideEvent() {
    popup.setHideOnEscape(true);
    popup.setVisible(true);
    let eventsPropagated = 0;
    events.listenOnce(
        dom.getElement('commonAncestor'), EventType.KEYDOWN, () => {
          ++eventsPropagated;
        });
    // Make a listener so that we stop hiding with an event handler.
    events.listenOnce(popup, PopupBase.EventType.BEFORE_HIDE, (e) => {
      e.preventDefault();
    });
    assertEquals(
        'The hide should have been cancelled', true, popup.isVisible());
    assertTrue(
        'The key event default action should not be prevented',
        testingEvents.fireKeySequence(targetDiv, KeyCodes.ESC));
    assertEquals('Keydown should have all propagated', 1, eventsPropagated);
  },

  testEscapeDismissalProvidesKeyTargetAsTargetForHideEvents() {
    popup.setHideOnEscape(true);
    popup.setVisible(true);
    let calls = 0;
    events.listenOnce(
        popup,
        [
          PopupBase.EventType.BEFORE_HIDE,
          PopupBase.EventType.HIDE,
        ],
        (e) => {
          calls++;
          assertEquals(
              'The key target should be the hide event target', 'targetDiv',
              e.target.id);
        });
    testingEvents.fireKeySequence(targetDiv, KeyCodes.ESC);
  },

  testAutoHide() {
    popup.setAutoHide(true);
    popup.setVisible(true);
    clock.tick(1000);  // avoid bouncing
    testingEvents.fireClickSequence(targetDiv);
    assertFalse(popup.isVisible());
  },

  testAutoHideCanBeDisabled() {
    popup.setAutoHide(false);
    popup.setVisible(true);
    clock.tick(1000);  // avoid bouncing
    testingEvents.fireClickSequence(targetDiv);
    assertTrue(
        'Should not be hidden if auto hide is disabled', popup.isVisible());
  },

  testAutoHideEnabledByDefault() {
    assertTrue(popup.getAutoHide());
  },

  testAutoHideWithPartners() {
    popup.setAutoHide(true);
    popup.setVisible(true);
    popup.addAutoHidePartner(targetDiv);
    popup.addAutoHidePartner(partnerDiv);
    clock.tick(1000);  // avoid bouncing

    testingEvents.fireClickSequence(targetDiv);
    assertTrue(popup.isVisible());
    testingEvents.fireClickSequence(partnerDiv);
    assertTrue(popup.isVisible());

    popup.removeAutoHidePartner(partnerDiv);
    testingEvents.fireClickSequence(partnerDiv);
    assertFalse(popup.isVisible());
  },

  testCanAddElementDuringBeforeShow() {
    popup.setElement(null);
    events.listenOnce(popup, PopupBase.EventType.BEFORE_SHOW, () => {
      popup.setElement(popupDiv);
    });
    popup.setVisible(true);
    assertTrue('Popup should be shown', popup.isVisible());
  },

  testShowWithNoElementThrowsException() {
    popup.setElement(null);
    const e = assertThrows(() => {
      popup.setVisible(true);
    });
    assertEquals(
        'Caller must call setElement before trying to show the popup',
        e.message);
  },

  testShowEventFiredWithNoTransition() {
    let showHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.SHOW, () => {
      showHandlerCalled = true;
    });

    popup.setVisible(true);
    assertTrue(showHandlerCalled);
  },

  testHideEventFiredWithNoTransition() {
    let hideHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.HIDE, () => {
      hideHandlerCalled = true;
    });

    popup.setVisible(true);
    popup.setVisible(false);
    assertTrue(hideHandlerCalled);
  },

  testOnShowTransition() {
    const mockTransition = new MockTransition();

    let showHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.SHOW, () => {
      showHandlerCalled = true;
    });

    popup.setTransition(mockTransition);
    popup.setVisible(true);
    assertTrue(mockTransition.wasPlayed);

    assertFalse(showHandlerCalled);
    mockTransition.dispatchEvent(Transition.EventType.END);
    assertTrue(showHandlerCalled);
  },

  testOnHideTransition() {
    const mockTransition = new MockTransition();

    let hideHandlerCalled = false;
    events.listen(popup, PopupBase.EventType.HIDE, () => {
      hideHandlerCalled = true;
    });

    popup.setTransition(undefined, mockTransition);
    popup.setVisible(true);
    assertFalse(mockTransition.wasPlayed);

    popup.setVisible(false);
    assertTrue(mockTransition.wasPlayed);

    assertFalse(hideHandlerCalled);
    mockTransition.dispatchEvent(Transition.EventType.END);
    assertTrue(hideHandlerCalled);
  },

  testSetVisibleWorksCorrectlyWithTransitions() {
    popup.setTransition(
        css3.fadeIn(popup.getElement(), 1),
        css3.fadeOut(popup.getElement(), 1));

    // Consecutive calls to setVisible works without needing to wait for
    // transition to finish.
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    popup.setVisible(false);
    assertFalse(popup.isVisible());
    clock.tick(1100);

    // Calling setVisible(true) immediately changed the state to visible.
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    clock.tick(1100);

    // Consecutive calls to setVisible, in opposite order.
    popup.setVisible(false);
    popup.setVisible(true);
    assertTrue(popup.isVisible());
    clock.tick(1100);

    // Calling setVisible(false) immediately changed the state to not visible.
    popup.setVisible(false);
    assertFalse(popup.isVisible());
    clock.tick(1100);
  },

  testWasRecentlyVisibleWorksCorrectlyWithTransitions() {
    popup.setTransition(
        css3.fadeIn(popup.getElement(), 1),
        css3.fadeOut(popup.getElement(), 1));

    popup.setVisible(true);
    clock.tick(1100);
    popup.setVisible(false);
    assertTrue(popup.isOrWasRecentlyVisible());
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    assertFalse(popup.isOrWasRecentlyVisible());
  },

  testMoveOffscreenRTL() {
    document.body.setAttribute('dir', 'rtl');
    popup.reposition = function() {
      this.element_.style.left = '100px';
      this.element_.style.top = '100px';
    };
    popup.setType(PopupBase.Type.MOVE_OFFSCREEN);
    popup.setElement(dom.getElement('moveOffscreenPopupDiv'));
    let originalScrollWidth = dom.getDocumentScrollElement().scrollWidth;
    popup.setVisible(true);
    popup.setVisible(false);
    assertFalse(
        'Moving a popup offscreen should not cause scrollbars',
        dom.getDocumentScrollElement().scrollWidth != originalScrollWidth);
  },

  testOnDocumentBlurDisabledCrossIframeDismissalWithoutDelay() {
    popup.setEnableCrossIframeDismissal(false);
    popup.setVisible(true);
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurDisabledCrossIframeDismissalWithDelay() {
    popup.setEnableCrossIframeDismissal(false);
    popup.setVisible(true);
    const e = new GoogTestingEvent(EventType.BLUR, document);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testOnDocumentBlurActiveElementInsidePopupWithoutDelay() {
    popup.setVisible(true);
    const elementInsidePopup = dom.createDom(TagName.DIV);
    dom.append(popupDiv, elementInsidePopup);
    elementInsidePopup.setAttribute('tabIndex', 0);
    elementInsidePopup.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testOnDocumentBlurActiveElementInsidePopupWithDelay() {
    popup.setVisible(true);
    const elementInsidePopup = dom.createDom(TagName.DIV);
    dom.append(popupDiv, elementInsidePopup);
    elementInsidePopup.setAttribute('tabIndex', 0);
    elementInsidePopup.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurActiveElementIsBodyWithoutDelay() {
    popup.setVisible(true);
    const bodyElement =
        dom.getDomHelper().getElementsByTagNameAndClass('body')[0];
    bodyElement.setAttribute('tabIndex', 0);
    bodyElement.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurActiveElementIsBodyWithDelay() {
    popup.setVisible(true);
    const bodyElement =
        dom.getDomHelper().getElementsByTagNameAndClass('body')[0];
    bodyElement.setAttribute('tabIndex', 0);
    bodyElement.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurEventTargetNotDocumentWithoutDelay() {
    popup.setVisible(true);
    const e = new GoogTestingEvent(EventType.BLUR, targetDiv);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurEventTargetNotDocumentWithDelay() {
    popup.setVisible(true);
    const e = new GoogTestingEvent(EventType.BLUR, targetDiv);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should remain visible', popup.isVisible());
  },

  testOnDocumentBlurShouldDebounceWithoutDelay() {
    popup.setVisible(true);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should be visible', popup.isVisible());
    dom.removeNode(focusDiv);
  },

  testOnDocumentBlurShouldNotDebounceWithDelay() {
    popup.setVisible(true);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertFalse('Popup should be invisible', popup.isVisible());
    dom.removeNode(focusDiv);
  },

  testOnDocumentBlurShouldNotHideBubbleWithoutDelay() {
    popup.setVisible(true);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should be visible', popup.isVisible());
    dom.removeNode(focusDiv);
  },

  testOnDocumentBlurShouldHideBubbleWithDelay() {
    popup.setVisible(true);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertFalse('Popup should be invisible', popup.isVisible());
    dom.removeNode(focusDiv);
  },

  testOnDocumentBlurShouldNotHideOnFocusAutoHidePartnerWithoutDelay() {
    popup.setVisible(true);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    popup.addAutoHidePartner(focusDiv);
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should be visible', popup.isVisible());
    popup.removeAutoHidePartner(focusDiv);
    dom.removeNode(focusDiv);
  },

  testOnDocumentBlurShouldNotHideOnFocusAutoHidePartnerWithDelay() {
    popup.setVisible(true);
    clock.tick(PopupBase.DEBOUNCE_DELAY_MS);
    const commonAncestor = dom.getElement('commonAncestor');
    const focusDiv = dom.createDom(TagName.DIV, 'tabIndex');
    popup.addAutoHidePartner(focusDiv);
    focusDiv.setAttribute('tabIndex', 0);
    dom.appendChild(commonAncestor, focusDiv);
    focusDiv.focus();
    const e = new GoogTestingEvent(EventType.BLUR, document);
    testingEvents.fireBrowserEvent(e);
    assertTrue('Popup should be visible', popup.isVisible());
    popup.removeAutoHidePartner(focusDiv);
    dom.removeNode(focusDiv);
  },
});

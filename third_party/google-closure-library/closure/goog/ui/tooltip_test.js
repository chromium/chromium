/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.TooltipTest');
goog.setTestOnly();

const AbsolutePosition = goog.require('goog.positioning.AbsolutePosition');
const Coordinate = goog.require('goog.math.Coordinate');
const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.events.EventType');
const FocusHandler = goog.require('goog.events.FocusHandler');
const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const PopupBase = goog.require('goog.ui.PopupBase');
const TagName = goog.require('goog.dom.TagName');
const TestQueue = goog.require('goog.testing.TestQueue');
const Tooltip = goog.require('goog.ui.Tooltip');
const events = goog.require('goog.testing.events');
const googDom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const userAgent = goog.require('goog.userAgent');

/**
 * A subclass of Tooltip that overrides `getPositioningStrategy`
 * for testing purposes.
 */
class TestTooltip extends Tooltip {
  constructor(el, text, dom) {
    super(el, text, dom);
  }

  /** @override */
  getPositioningStrategy() {
    return new AbsolutePosition(13, 17);
  }
}

let tt;
let clock;
let handler;
let eventQueue;
let dom;

// Allow positions to be off by one in gecko as it reports scrolling
// offsets in steps of 2.
const ALLOWED_OFFSET = userAgent.GECKO ? 1 : 0;

testSuite({
  setUp() {
    // We get access denied error when accessing the iframe in IE on the farm
    // as IE doesn't have the same window size issues as firefox on the farm
    // we bypass the iframe and use the current document instead.
    if (userAgent.EDGE_OR_IE) {
      dom = googDom.getDomHelper(document);
    } else {
      const frame = document.getElementById('testframe');
      const doc = googDom.getFrameContentDocument(frame);
      dom = googDom.getDomHelper(doc);
    }

    // Host elements in fixed size iframe to avoid window size problems when
    // running under Selenium.
    dom.getDocument().body.innerHTML = '<p id="notpopup">Content</p>' +
        '<p id="hovertarget">Hover Here For Popup</p>' +
        '<p id="second">Secondary target</p>';

    tt = new Tooltip(undefined, undefined, dom);
    tt.setElement(dom.createDom(
        TagName.DIV, {id: 'popup', style: 'visibility:hidden'}, 'Hello'));
    clock = new MockClock(true);
    eventQueue = new TestQueue();
    handler = new EventHandler(eventQueue);
    handler.listen(tt, PopupBase.EventType.SHOW, eventQueue.enqueue);
    handler.listen(tt, PopupBase.EventType.HIDE, eventQueue.enqueue);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  tearDown() {
    // tooltip needs to be hidden as well as disposed of so that it doesn't
    // leave global state hanging around to trip up other tests.
    tt.onHide();
    tt.dispose();
    clock.uninstall();
    handler.removeAll();
  },

  testConstructor() {
    const element = tt.getElement();
    assertNotNull('Tooltip should have non-null element', element);
    assertEquals(
        'Tooltip element should be the DIV we created', dom.getElement('popup'),
        element);
    assertEquals(
        'Tooltip element should be a child of the document body',
        dom.getDocument().body, element.parentNode);
  },

  testTooltipShowsAndHides() {
    const hoverTarget = dom.getElement('hovertarget');
    const elsewhere = dom.getElement('notpopup');
    const element = tt.getElement();
    const position = new Coordinate(5, 5);
    assertNotNull('Tooltip should have non-null element', element);
    assertEquals(
        'Initial state should be inactive', Tooltip.State.INACTIVE,
        tt.getState());
    tt.attach(hoverTarget);
    tt.setShowDelayMs(100);
    tt.setHideDelayMs(50);
    events.fireMouseOverEvent(hoverTarget, elsewhere, position);
    assertEquals(Tooltip.State.WAITING_TO_SHOW, tt.getState());
    clock.tick(101);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals(
        'tooltip y position (10px margin below the cursor)', '15px',
        tt.getElement().style.top);
    assertEquals(Tooltip.State.SHOWING, tt.getState());
    assertEquals(PopupBase.EventType.SHOW, eventQueue.dequeue().type);
    assertTrue(eventQueue.isEmpty());

    events.fireMouseOutEvent(hoverTarget, elsewhere);
    assertEquals(Tooltip.State.WAITING_TO_HIDE, tt.getState());
    clock.tick(51);
    assertEquals('hidden', tt.getElement().style.visibility);
    assertEquals(Tooltip.State.INACTIVE, tt.getState());
    assertEquals(PopupBase.EventType.HIDE, eventQueue.dequeue().type);
    assertTrue(eventQueue.isEmpty());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMultipleTargets() {
    const firstTarget = dom.getElement('hovertarget');
    const secondTarget = dom.getElement('second');
    const elsewhere = dom.getElement('notpopup');
    const element = tt.getElement();

    tt.attach(firstTarget);
    tt.attach(secondTarget);
    tt.setShowDelayMs(100);
    tt.setHideDelayMs(50);

    // Move over first target
    events.fireMouseOverEvent(firstTarget, elsewhere);
    clock.tick(101);
    assertEquals(PopupBase.EventType.SHOW, eventQueue.dequeue().type);
    assertTrue(eventQueue.isEmpty());

    // Move from first to second
    events.fireMouseOutEvent(firstTarget, secondTarget);
    events.fireMouseOverEvent(secondTarget, firstTarget);
    assertEquals(Tooltip.State.UPDATING, tt.getState());
    assertTrue(eventQueue.isEmpty());

    // Move from second to element (before second shows)
    events.fireMouseOutEvent(secondTarget, element);
    events.fireMouseOverEvent(element, secondTarget);
    assertEquals(Tooltip.State.SHOWING, tt.getState());
    assertTrue(eventQueue.isEmpty());

    // Move from element to second, and let it show
    events.fireMouseOutEvent(element, secondTarget);
    events.fireMouseOverEvent(secondTarget, element);
    assertEquals(Tooltip.State.UPDATING, tt.getState());
    clock.tick(101);
    assertEquals(Tooltip.State.SHOWING, tt.getState());
    assertEquals('Anchor should be second target', secondTarget, tt.anchor);
    assertEquals(PopupBase.EventType.HIDE, eventQueue.dequeue().type);
    assertEquals(PopupBase.EventType.SHOW, eventQueue.dequeue().type);
    assertTrue(eventQueue.isEmpty());

    // Move from second to first and then off without first showing
    events.fireMouseOutEvent(secondTarget, firstTarget);
    events.fireMouseOverEvent(firstTarget, secondTarget);
    assertEquals(Tooltip.State.UPDATING, tt.getState());
    events.fireMouseOutEvent(firstTarget, elsewhere);
    assertEquals(Tooltip.State.WAITING_TO_HIDE, tt.getState());
    clock.tick(51);
    assertEquals('hidden', tt.getElement().style.visibility);
    assertEquals(Tooltip.State.INACTIVE, tt.getState());
    assertEquals(PopupBase.EventType.HIDE, eventQueue.dequeue().type);
    assertTrue(eventQueue.isEmpty());
    clock.tick(200);

    // Move from element to second, but detach second before it shows.
    events.fireMouseOutEvent(element, secondTarget);
    events.fireMouseOverEvent(secondTarget, element);
    assertEquals(Tooltip.State.WAITING_TO_SHOW, tt.getState());
    tt.detach(secondTarget);
    clock.tick(200);
    assertEquals(Tooltip.State.INACTIVE, tt.getState());
    assertEquals('Anchor should be second target', secondTarget, tt.anchor);
    assertTrue(eventQueue.isEmpty());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRequireInteraction() {
    const hoverTarget = dom.getElement('hovertarget');
    const elsewhere = dom.getElement('notpopup');

    tt.attach(hoverTarget);
    tt.setShowDelayMs(100);
    tt.setHideDelayMs(50);
    tt.setRequireInteraction(true);

    events.fireMouseOverEvent(hoverTarget, elsewhere);
    clock.tick(101);
    assertEquals(
        'Tooltip should not show without mouse move event', 'hidden',
        tt.getElement().style.visibility);
    events.fireMouseMoveEvent(hoverTarget);
    events.fireMouseOverEvent(hoverTarget, elsewhere);
    clock.tick(101);
    assertEquals(
        'Tooltip should show because we had mouse move event', 'visible',
        tt.getElement().style.visibility);

    events.fireMouseOutEvent(hoverTarget, elsewhere);
    clock.tick(51);
    assertEquals('hidden', tt.getElement().style.visibility);
    events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, hoverTarget));
    clock.tick(101);
    assertEquals(
        'Tooltip should show because we had focus event', 'visible',
        tt.getElement().style.visibility);
    events.fireBrowserEvent(new GoogEvent(EventType.BLUR, hoverTarget));
    clock.tick(51);
    assertEquals('hidden', tt.getElement().style.visibility);

    events.fireMouseMoveEvent(hoverTarget);
    events.fireMouseOverEvent(hoverTarget, elsewhere);
    events.fireMouseOutEvent(hoverTarget, elsewhere);
    events.fireMouseOverEvent(hoverTarget, elsewhere);
    clock.tick(101);
    assertEquals(
        'A cancelled trigger should also cancel the seen interaction', 'hidden',
        tt.getElement().style.visibility);
  },

  testDispose() {
    const element = tt.getElement();
    tt.dispose();
    assertTrue('Tooltip should have been disposed of', tt.isDisposed());
    assertNull(
        'Tooltip element reference should have been nulled out',
        tt.getElement());
    assertNotEquals(
        'Tooltip element should not be a child of the body', document.body,
        element.parentNode);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testNested() {
    let ttNested;
    tt.getElement().appendChild(
        dom.createDom(TagName.SPAN, {id: 'nested'}, 'Goodbye'));
    ttNested = new Tooltip(undefined, undefined, dom);
    ttNested.setElement(dom.createDom(TagName.DIV, {id: 'nestedPopup'}, 'hi'));
    tt.setShowDelayMs(100);
    tt.setHideDelayMs(50);
    ttNested.setShowDelayMs(75);
    ttNested.setHideDelayMs(25);
    const nestedAnchor = dom.getElement('nested');
    const hoverTarget = dom.getElement('hovertarget');
    const outerTooltip = dom.getElement('popup');
    const innerTooltip = dom.getElement('nestedPopup');
    const elsewhere = dom.getElement('notpopup');

    ttNested.attach(nestedAnchor);
    tt.attach(hoverTarget);

    // Test mouse into, out of nested tooltip
    events.fireMouseOverEvent(hoverTarget, elsewhere);
    clock.tick(101);
    events.fireMouseOutEvent(hoverTarget, outerTooltip);
    events.fireMouseOverEvent(outerTooltip, hoverTarget);
    clock.tick(51);
    assertEquals('visible', tt.getElement().style.visibility);
    events.fireMouseOutEvent(outerTooltip, nestedAnchor);
    events.fireMouseOverEvent(nestedAnchor, outerTooltip);
    clock.tick(76);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals('visible', ttNested.getElement().style.visibility);
    events.fireMouseOutEvent(nestedAnchor, outerTooltip);
    events.fireMouseOverEvent(outerTooltip, nestedAnchor);
    clock.tick(100);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals('hidden', ttNested.getElement().style.visibility);

    // Go back in nested tooltip and then out through tooltip element.
    events.fireMouseOutEvent(outerTooltip, nestedAnchor);
    events.fireMouseOverEvent(nestedAnchor, outerTooltip);
    clock.tick(76);
    events.fireMouseOutEvent(nestedAnchor, innerTooltip);
    events.fireMouseOverEvent(innerTooltip, nestedAnchor);
    clock.tick(15);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals('visible', ttNested.getElement().style.visibility);
    events.fireMouseOutEvent(innerTooltip, elsewhere);
    clock.tick(26);
    assertEquals('hidden', ttNested.getElement().style.visibility);
    clock.tick(51);
    assertEquals('hidden', tt.getElement().style.visibility);

    // Test with focus
    events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, hoverTarget));
    clock.tick(101);
    events.fireBrowserEvent(new GoogEvent(EventType.BLUR, hoverTarget));
    events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, nestedAnchor));
    clock.tick(76);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals('visible', ttNested.getElement().style.visibility);
    events.fireBrowserEvent(new GoogEvent(EventType.BLUR, nestedAnchor));
    events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, hoverTarget));
    clock.tick(26);
    assertEquals('visible', tt.getElement().style.visibility);
    assertEquals('hidden', ttNested.getElement().style.visibility);

    ttNested.onHide();
    ttNested.dispose();
  },

  testPosition() {
    dom.getDocument().body.style.paddingBottom = '150%';  // force scrollbar
    const scrollEl = dom.getDocumentScrollElement();

    const anchor = dom.getElement('hovertarget');
    const tooltip = new Tooltip(anchor, 'foo');
    tooltip.getElement().style.position = 'absolute';

    /** @suppress {visibility} suppression added to enable type checking */
    tooltip.cursorPosition.x = 100;
    /** @suppress {visibility} suppression added to enable type checking */
    tooltip.cursorPosition.y = 100;
    tooltip.showForElement(anchor);

    assertEquals(
        'Tooltip should be at cursor position',
        '(110, 110)',  // (100, 100) + padding (10, 10)
        style.getPageOffset(tooltip.getElement()).toString());

    scrollEl.scrollTop = 50;

    const offset = style.getPageOffset(tooltip.getElement());
    assertTrue(
        'Tooltip should be at cursor position when scrolled',
        Math.abs(offset.x - 110) <= ALLOWED_OFFSET);  // 100 + padding 10
    assertTrue(
        'Tooltip should be at cursor position when scrolled',
        Math.abs(offset.y - 110) <= ALLOWED_OFFSET);  // 100 + padding 10

    tooltip.dispose();
    dom.getDocument().body.style.paddingTop = '';
    scrollEl.scrollTop = 0;
  },

  testPositionOverride() {
    const anchor = dom.getElement('hovertarget');
    const tooltip = new TestTooltip(anchor, 'foo', dom);

    tooltip.showForElement(anchor);

    assertEquals(
        'Tooltip should be at absolute position', '(13, 17)',
        style.getPageOffset(tooltip.getElement()).toString());
    tooltip.dispose();
  },

  testHtmlContent() {
    tt.setSafeHtml(
        testing.newSafeHtmlForTest('<span class="theSpan">Hello</span>'));
    const spanEl = googDom.getElementByClass('theSpan', tt.getElement());
    assertEquals('Hello', googDom.getTextContent(spanEl));
  },

  testSetElementNull() {
    tt.setElement(null);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFocusBlurElementsInTooltip() {
    const anchorEl = dom.getElement('hovertarget');
    googDom.setFocusableTabIndex(anchorEl, true);
    tt.attach(anchorEl);
    events.fireFocusEvent(anchorEl);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    events.fireBlurEvent(anchorEl);
    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSIN);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    // Run blur on the previous element followed by focus on the element being
    // focused, as would normally happen when focus() is called on an element.
    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSOUT);
    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSIN);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSOUT);
    clock.tick(1000);
    assertEquals('hidden', tt.getElement().style.visibility);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFocusElementInTooltipThenBackToAnchor() {
    const anchorEl = dom.getElement('hovertarget');
    googDom.setFocusableTabIndex(anchorEl, true);
    tt.attach(anchorEl);
    events.fireFocusEvent(anchorEl);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    // Run blur on the previous element followed by focus on the element being
    // focused, as would normally happen when focus() is called on an element.
    events.fireBlurEvent(anchorEl);
    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSIN);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    // Run blur on the previous element followed by focus on the element being
    // focused, as would normally happen when focus() is called on an element.
    tt.tooltipFocusHandler_.dispatchEvent(FocusHandler.EventType.FOCUSOUT);
    events.fireFocusEvent(anchorEl);
    clock.tick(1000);
    assertEquals('visible', tt.getElement().style.visibility);

    events.fireBlurEvent(anchorEl);
    clock.tick(1000);
    assertEquals('hidden', tt.getElement().style.visibility);
  },
});

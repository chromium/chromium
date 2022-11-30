/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.IframeMaskTest');
goog.setTestOnly();

const IframeMask = goog.require('goog.ui.IframeMask');
const MockClock = goog.require('goog.testing.MockClock');
const Pool = goog.require('goog.structs.Pool');
const Popup = goog.require('goog.ui.Popup');
const PopupBase = goog.require('goog.ui.PopupBase');
const StrictMock = goog.require('goog.testing.StrictMock');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const domIframe = goog.require('goog.dom.iframe');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let iframeMask;
let mockClock;

function findOneAndOnlyIframe() {
  const iframes = dom.getElementsByTagName(TagName.IFRAME);
  assertEquals(
      'There should be exactly 1 iframe in the document', 1, iframes.length);
  return iframes[0];
}

function assertNoIframes() {
  assertEquals(
      'Expected no iframes in the document', 0,
      dom.getElementsByTagNameAndClass(TagName.IFRAME).length);
}

testSuite({
  setUp() {
    dom.getElement('sandbox').innerHTML = '<div id="popup"></div>';
    mockClock = new MockClock(true);

    iframeMask = new IframeMask();
  },

  tearDown() {
    iframeMask.dispose();
    mockClock.dispose();

    assertNoIframes();
  },

  testApplyFullScreenMask() {
    iframeMask.applyMask();

    const iframe = findOneAndOnlyIframe();
    assertEquals('block', iframe.style.display);
    assertEquals('absolute', iframe.style.position);

    // coerce zindex to a string
    assertEquals('1', iframe.style.zIndex + '');

    iframeMask.hideMask();
    assertEquals('none', iframe.style.display);
  },

  testApplyOpacity() {
    iframeMask.setOpacity(0.3);
    iframeMask.applyMask();

    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      assertContains(
          'Expected opactity to be set in the CSS style', '30',
          findOneAndOnlyIframe().style.cssText);
    } else {
      assertContains(
          'Expected opactity to be set in the CSS style', '0.3',
          findOneAndOnlyIframe().style.cssText);
    }
  },

  testApplyZIndex() {
    iframeMask.setZIndex(5);
    iframeMask.applyMask();

    // coerce zindex to a string
    assertEquals('5', findOneAndOnlyIframe().style.zIndex + '');
  },

  testSnapElement() {
    iframeMask.setSnapElement(dom.getElement('popup'));
    iframeMask.applyMask();

    const iframe = findOneAndOnlyIframe();
    const bounds = style.getBounds(iframe);
    assertEquals(100, bounds.left);
    assertEquals(900, bounds.top);
    assertEquals(300, bounds.width);
    assertEquals(400, bounds.height);

    iframeMask.setSnapElement(document.documentElement);

    // Make sure that snapping to a different element changes the bounds.
    assertNotEquals(
        'Snap element not updated', 400, style.getBounds(iframe).height);
  },

  testAttachToPopup() {
    const popup = new Popup(dom.getElement('popup'));
    iframeMask.listenOnTarget(
        popup, PopupBase.EventType.SHOW, PopupBase.EventType.HIDE,
        dom.getElement('popup'));

    assertNoIframes();
    popup.setVisible(true);
    assertNoIframes();

    // Tick because the showing of the iframe mask happens asynchronously.
    // (Otherwise the handling of the mousedown can take so long that a bounce
    // occurs).
    mockClock.tick(1);

    const iframe = findOneAndOnlyIframe();
    const bounds = style.getBounds(iframe);
    assertEquals(300, bounds.width);
    assertEquals(400, bounds.height);
    assertEquals('block', iframe.style.display);

    popup.setVisible(false);
    assertEquals('none', iframe.style.display);
  },

  testQuickHidingPopup() {
    const popup = new Popup(dom.getElement('popup'));
    iframeMask.listenOnTarget(
        popup, PopupBase.EventType.SHOW, PopupBase.EventType.HIDE);

    assertNoIframes();
    popup.setVisible(true);
    assertNoIframes();
    popup.setVisible(false);
    assertNoIframes();

    // Tick because the showing of the iframe mask happens asynchronously.
    // (Otherwise the handling of the mousedown can take so long that a bounce
    // occurs).
    mockClock.tick(1);
    assertNoIframes();
  },

  testRemoveHandlers() {
    const popup = new Popup(dom.getElement('popup'));
    iframeMask.listenOnTarget(
        popup, PopupBase.EventType.SHOW, PopupBase.EventType.HIDE);
    iframeMask.removeHandlers();
    popup.setVisible(true);

    // Tick because the showing of the iframe mask happens asynchronously.
    // (Otherwise the handling of the mousedown can take so long that a bounce
    // occurs).
    mockClock.tick(1);
    assertNoIframes();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testIframePool() {
    const iframe = domIframe.createBlank(dom.getDomHelper());
    const mockPool = new StrictMock(Pool);
    mockPool.getObject();
    mockPool.$returns(iframe);

    mockPool.$replay();

    iframeMask.dispose();

    // Create a new iframe mask with a pool, and verify that it checks
    // its iframe out of the pool instead of creating one.
    /** @suppress {checkTypes} suppression added to enable type checking */
    iframeMask = new IframeMask(null, mockPool);
    iframeMask.applyMask();
    mockPool.$verify();
    findOneAndOnlyIframe();

    mockPool.$reset();

    mockPool.releaseObject(iframe);
    mockPool.$replay();

    // When the iframe mask has a pool, the pool is responsible for
    // removing the iframe from the DOM.
    iframeMask.hideMask();
    mockPool.$verify();
    findOneAndOnlyIframe();

    // And showing the iframe again should check it out of the pool again.
    mockPool.$reset();
    mockPool.getObject();
    mockPool.$returns(iframe);
    mockPool.$replay();

    iframeMask.applyMask();
    mockPool.$verify();

    // When the test is over, the iframe mask should be disposed. Make sure
    // that the pool removes the iframe from the page.
    mockPool.$reset();
    mockPool.releaseObject(iframe);
    mockPool.$does(() => {
      dom.removeNode(iframe);
    });
    mockPool.$replay();
  },
});

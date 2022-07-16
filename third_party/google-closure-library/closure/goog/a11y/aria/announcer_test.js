/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.a11y.aria.AnnouncerTest');
goog.setTestOnly();

const Announcer = goog.require('goog.a11y.aria.Announcer');
const LivePriority = goog.require('goog.a11y.aria.LivePriority');
const MockClock = goog.require('goog.testing.MockClock');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const asserts = goog.require('goog.asserts');
const googArray = goog.require('goog.array');
const googDispose = goog.require('goog.dispose');
const googDom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const iframe = goog.require('goog.dom.iframe');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let someDiv;
let someSpan;
let mockClock;

function getLiveRegion(priority, domHelper = undefined) {
  const dom = domHelper || googDom.getDomHelper();
  const divs = dom.getElementsByTagNameAndClass(TagName.DIV, null);
  const liveRegions = [];
  googArray.forEach(divs, (div) => {
    if (aria.getState(div, 'live') == priority) {
      liveRegions.push(div);
    }
  });
  assertEquals(1, liveRegions.length);
  return liveRegions[0];
}

function checkLiveRegionContains(text, priority, domHelper = undefined) {
  const liveRegion = getLiveRegion(priority, domHelper);
  mockClock.tick(1);
  assertEquals(text, googDom.getTextContent(liveRegion));
}
testSuite({
  setUp() {
    sandbox = asserts.assert(googDom.getElement('sandbox'));
    someDiv = googDom.createDom(TagName.DIV, {id: 'someDiv'}, 'DIV');
    someSpan = googDom.createDom(TagName.SPAN, {id: 'someSpan'}, 'SPAN');
    sandbox.appendChild(someDiv);
    someDiv.appendChild(someSpan);

    mockClock = new MockClock(true);
  },

  tearDown() {
    googDom.removeChildren(sandbox);
    someDiv = null;
    someSpan = null;

    googDispose(mockClock);
  },

  testAnnouncerAndDispose() {
    const text = 'test content';
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say(text);
    checkLiveRegionContains(text, 'polite');
    googDispose(announcer);
  },

  testAnnouncerTwice() {
    const text = 'test content1';
    const text2 = 'test content2';
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say(text);
    announcer.say(text2);
    checkLiveRegionContains(text2, 'polite');
    googDispose(announcer);
  },

  testAnnouncerTwiceSameMessage() {
    const text = 'test content';
    const repeatedText = text + googString.Unicode.NBSP;
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say(text);
    const firstLiveRegion = getLiveRegion('polite');
    announcer.say(text, undefined);
    const secondLiveRegion = getLiveRegion('polite');
    assertEquals(firstLiveRegion, secondLiveRegion);
    checkLiveRegionContains(repeatedText, 'polite');
    googDispose(announcer);
  },

  testAnnouncerAssertive() {
    const text = 'test content';
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say(text, LivePriority.ASSERTIVE);
    checkLiveRegionContains(text, 'assertive');
    googDispose(announcer);
  },

  testAnnouncerInIframe() {
    const text = 'test content';
    const frame = iframe.createWithContent(sandbox);
    const helper =
        googDom.getDomHelper(googDom.getFrameContentDocument(frame).body);
    const announcer = new Announcer(helper);
    announcer.say(text, /** @type {?} */ ('polite'));
    checkLiveRegionContains(text, 'polite', helper);
    googDispose(announcer);
  },

  testAnnouncerWithAriaHidden() {
    const text = 'test content1';
    const text2 = 'test content2';
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say(text);
    // Set aria-hidden attribute on the live region (simulates a modal dialog
    // being opened).
    const liveRegion = getLiveRegion('polite');
    aria.setState(liveRegion, State.HIDDEN, true);

    // Announce a new message and make sure that the aria-hidden was removed.
    announcer.say(text2);
    checkLiveRegionContains(text2, 'polite');
    assertEquals('', aria.getState(liveRegion, State.HIDDEN));
    googDispose(announcer);
  },

  testAnnouncerSetsAndReturnsId() {
    const announcer = new Announcer(googDom.getDomHelper());
    announcer.say('test');

    // Read the dom to find the id
    const domLiveRegionId = getLiveRegion('polite').getAttribute('id');

    assertEquals(
        announcer.getLiveRegionId(LivePriority.POLITE), domLiveRegionId);
    googDispose(announcer);
  },
});

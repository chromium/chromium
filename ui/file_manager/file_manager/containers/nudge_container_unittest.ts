// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';
import type {XfNudge} from '../widgets/xf_nudge.js';

import {NudgeContainer, nudgeInfo, NudgeType} from './nudge_container.js';

/**
 * Holds all the elements, used to easily clear the DOM in between tests.
 */
const nudgeHolder: HTMLElement = document.createElement('div');

/**
 * An instance of the `NudgeContainer`, this is reset in between every test.
 */
let nudgeContainer: NudgeContainer|undefined;

/**
 * The <xf-nudge> element that is appended to the page at the start of every
 * test.
 */
let nudgeElement: XfNudge|undefined;

/**
 * Save a copy of the test Nudge so that any test overwrites can be restored in
 * between tests. For example if the expiry date needs to be overwritten it is
 * restored for the next test.
 */
const savedNudgeInfo = {...nudgeInfo[NudgeType.TEST_NUDGE]};

/**
 * Sets up the initial holder that is used for every test.
 */
export function setUpPage() {
  document.body.appendChild(nudgeHolder);
}

/**
 * Creates new <xf-nudge> element for each test.
 */
export function setUp() {
  nudgeInfo[NudgeType.TEST_NUDGE] = {...savedNudgeInfo};
  nudgeElement = document.createElement('xf-nudge');
  nudgeHolder.appendChild(nudgeElement);
  nudgeContainer = new NudgeContainer();
}

/**
 * Clears the <xf-nudge> between each test to ensure a clean slate.
 */
export function tearDown() {
  nudgeHolder.innerText = '';

  // Ensure to close the nudge before clearing the container as the idle
  // callback needs to be properly shutdown.
  nudgeContainer!.closeNudge(NudgeType.TEST_NUDGE);
  nudgeContainer = undefined;
  nudgeElement = undefined;
  window.localStorage.clear();
}

/**
 * Create a <div id="test"> which anchors the `NudgeType.TEST_NUDGE`.
 */
async function createAndAppendTestDiv() {
  const testDiv = document.createElement('div');
  testDiv.id = 'test';
  testDiv.style.width = '100px';
  testDiv.style.height = '100px';
  testDiv.style.position = 'absolute';
  nudgeHolder.appendChild(testDiv);
  await waitUntil(() => {
    const boundingBox = testDiv.getBoundingClientRect();
    return boundingBox.width === 100 && boundingBox.height === 100;
  });
  return testDiv;
}

/**
 * Helper method to get the <p id="nudge-content"> element that is added to the
 * page to describe the content in the nudge.
 */
function getDescribedByElement(): HTMLElement|null {
  return document.querySelector('p#nudge-content');
}

/**
 * Wait until the desired number of repositions is met.
 */
function waitUntilRepositions(repositions: number) {
  return waitUntil(() => nudgeElement!.repositions === repositions);
}

/**
 * The repositions are setup as 0, this indicates the nudge has not been moved
 * to position, i.e. an uninitialised state.
 */
function waitUntilRepositionsUninitialised() {
  return waitUntilRepositions(0);
}

/**
 * Creates the test anchor and nudge and waits until the repositions are 1.
 */
async function createAndShowTestNudge() {
  await createAndAppendTestDiv();
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositions(1);
}

/**
 * Tests that a defined nudge without an anchor is not shown.
 */
export async function testShowWorksOnlyWhenAProperAnchorIsAvailable(
    done: () => void) {
  // The first showing of nudge should not work as the <div id="test"> is not
  // visible on the DOM.
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  assertFalse(await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE));
  await waitUntilRepositionsUninitialised();

  // The second showing of the nudge should work as we've appended the <div
  // id="test"> to the DOM.
  await createAndAppendTestDiv();
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositions(1);

  done();
}

/**
 * Tests that the enter key dismisses the nudge.
 */
export async function testEnterKeyHidesNudge(done: () => void) {
  nudgeInfo[NudgeType.TEST_NUDGE].selfDismiss = false;
  await createAndShowTestNudge();

  const keyDownEvent = new KeyboardEvent('keydown', {key: 'Enter'});
  document.dispatchEvent(keyDownEvent);

  // No new repositions should be called and the check seen property should be
  // true.
  await waitUntilRepositions(1);
  assertTrue(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      'check nudge has been seen');

  done();
}

/**
 * Tests that a <p> element is appended beside the anchor element with the nudge
 * content to enable screen readers to hear the content.
 */
export async function testAriaDescribedByElementIsAdded(done: () => void) {
  await createAndShowTestNudge();

  await waitUntil(() => getDescribedByElement() !== null);
  const describedByElement: HTMLElement|null =
      document.querySelector('p#nudge-content');

  assertNotEquals(describedByElement, null);
  assertEquals(
      describedByElement!.innerText, nudgeInfo[NudgeType.TEST_NUDGE].content());

  done();
}

/**
 * Tests that the nudge moves with the element if it gets moved
 * programmatically.
 */
export async function testNudgeMovesWhenElementIsRepositioned(
    done: () => void) {
  const testDiv = await createAndAppendTestDiv();
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositions(1);

  // Moving the `testDiv` should toggle the repositioning logic.
  testDiv.style.left = '200px';
  testDiv.style.top = '200px';
  await waitUntilRepositions(2);

  done();
}

/**
 * Tests that the nudge is not shown after being shown for the first time.
 */
export async function testNudgeIsNotShownAfterFirstTime(done: () => void) {
  await createAndShowTestNudge();

  // Close the nudge which should set the nudge to "seen".
  nudgeContainer!.closeNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositionsUninitialised();
  assertTrue(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      'check nudge has been seen');

  // Assert that showing the nudge again doesn't work as it's already been
  // "seen".
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositionsUninitialised();

  done();
}

/**
 * Tests the nudge doesn't show if the expiry period has elapsed.
 */
export async function testNudgeIsNotShownIfExpiryPeriodElapsed(
    done: () => void) {
  // Update the test nudge timestamp to be 60s before now.
  nudgeInfo[NudgeType.TEST_NUDGE].expiryDate =
      new Date(new Date().getTime() - (60 * 1000));
  await createAndAppendTestDiv();
  nudgeContainer!.showNudge(NudgeType.TEST_NUDGE);
  await waitUntilRepositionsUninitialised();

  done();
}

/**
 * Tests the nudge is dismissed by clicking on the nudge.
 */
export async function testNudgeDismissButton(done: () => void) {
  nudgeInfo[NudgeType.TEST_NUDGE].selfDismiss = true;
  await createAndShowTestNudge();

  // Click and wait it to dismiss.
  nudgeElement!.dispatchEvent(new PointerEvent('pointerdown'));

  // Reposition to hidden.
  await waitUntilRepositionsUninitialised();
  assertTrue(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      'check nudge has been seen');

  done();
}

/**
 * Tests the nudge is dismissed by clicking on the anchor.
 */
export async function testNudgeDismissAnchor(done: () => void) {
  nudgeInfo[NudgeType.TEST_NUDGE].selfDismiss = true;
  await createAndShowTestNudge();

  // Click and wait it to dismiss.
  const anchor = nudgeInfo[NudgeType.TEST_NUDGE].anchor();
  anchor!.dispatchEvent(new PointerEvent('pointerdown'));

  // Reposition to hidden.
  await waitUntilRepositionsUninitialised();
  assertTrue(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      'check nudge has been seen');

  done();
}

/**
 * Tests the nudge using the dismissOnKeyDown().
 */
export async function testNudgeDismissKeyDown(done: () => void) {
  nudgeInfo[NudgeType.TEST_NUDGE].selfDismiss = true;
  nudgeInfo[NudgeType.TEST_NUDGE].dismissOnKeyDown =
      (_, event: KeyboardEvent) => {
        // In tests we can send the keydown directly to the nudge.
        if (event.target === nudgeElement) {
          return true;
        }
        return false;
      };
  await createAndShowTestNudge();

  // Send a keydown somewhere else, should not dismiss.
  document.body.dispatchEvent(new KeyboardEvent('keydown', {bubbles: true}));
  assertFalse(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      `nudge shouldn't be dismissed by keydown on <body>`);

  // Send keydown to the nudge.
  nudgeElement!.dispatchEvent(new KeyboardEvent('keydown', {bubbles: true}));

  // Reposition to hidden.
  await waitUntilRepositionsUninitialised();
  assertTrue(
      await nudgeContainer!.checkSeen(NudgeType.TEST_NUDGE),
      'check nudge has been seen');

  done();
}

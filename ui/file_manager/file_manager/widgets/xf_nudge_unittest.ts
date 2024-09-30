// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './xf_nudge.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertGT, assertLT, assertThrows, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {NudgeDirection, XfNudge} from './xf_nudge.js';

const nudgeContainer: HTMLElement = document.createElement('div');

export function setUpPage() {
  document.body.appendChild(nudgeContainer);
}

/**
 * Creates new <xf-nudge> element for each test.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-nudge></xf-nudge>
`;
}

/**
 * Clears the <xf-nudge> between each test to ensure a clean slate.
 */
export function tearDown() {
  nudgeContainer.innerText = '';
}

/**
 * Returns the <xf-nudge> component in the DOM.
 */
function getNudge(): XfNudge {
  return document.querySelector<XfNudge>('xf-nudge')!;
}

/**
 * Tests that show doesn't work if content or anchor is not set.
 */
export function testShowDoesntWorkIfContentOrAnchorNotSet() {
  const nudge = getNudge();

  // The nudge should throw an error without setting content or anchor.
  assertThrows(nudge.show);

  // The nudge should throw when content is added but no anchor.
  nudge.content = 'adding content';
  assertThrows(nudge.show);

  // After setting an anchor, show should not throw.
  const anchor = document.createElement('div');
  nudge.insertAdjacentElement('afterend', anchor);
  nudge.anchor = anchor;
  nudge.direction = NudgeDirection.BOTTOM_ENDWARD;
  nudge.show();

  // The nudge should be positioned in the viewport somewhere (the next tests
  // asserts that somewhere).
  assertGT(nudge.dotRect.x, 0);
  assertGT(nudge.dotRect.y, 0);
  assertGT(nudge.bubbleRect.x, 0);
  assertGT(nudge.bubbleRect.y, 0);
}

/**
 * Tests that the dot and bubble are appropriately set given the location of the
 * anchor element.
 */
export function testNudgeDotAndBubbleIsPositionedInTheRightPlace() {
  const nudge = getNudge();

  // Create an anchor element and insert it before the nudge in the DOM.
  const anchor = document.createElement('div');
  anchor.style.width = '100px';
  anchor.style.height = '100px';
  anchor.innerText = 'Test anchor';
  nudge.insertAdjacentElement('beforebegin', anchor);

  // Update the nudge contents, direction and anchor it to the element created
  // above.
  nudge.content = 'Nudge contents';
  nudge.direction = NudgeDirection.BOTTOM_ENDWARD;
  nudge.anchor = anchor;
  nudge.show();

  // The y-ordinate for the dot must be lower than the height of the anchor
  // (100px) + the height of the dot (8px) + a predefined gap (4px)
  assertEquals(nudge.dotRect.y, 112, 'dot y-ordinate');

  // The x-ordinate for the dot must be half of the anchor element (50px) + half
  // the width of the dot (4px) to ensure if it is positioned at the screen edge
  // it doesn't appear half off the edge.
  assertEquals(nudge.dotRect.x, 54, 'dot x-ordinate');

  // The y-ordinate for the bubble must start lower than the dot + height of the
  // dot (8px) + a predetermined gap (4px)
  assertGT(nudge.bubbleRect.y, 112, 'bubble y-ordinate');

  // The x-ordinate for the bubble must be less than the x-ordinate for the dot
  // as it uses the `NudgeDirection.BOTTOM_ENDWARD` direction which tries to
  // show directly under the dot and extend evenly unless that extends past the
  // window width.
  assertLT(nudge.bubbleRect.x, 54, 'bubble x-ordinate');
}

/**
 * Tests that the nudge gets repositioned appopriately if the element moves.
 */
export function testNudgeGetsRepositionedCorrectlyIfAnchorChanges() {
  const nudge = getNudge();

  // Insert an anchor and make it position relative to ensure we can position it
  // using the left and right style attributes.
  const anchor = document.createElement('div');
  anchor.style.width = '100px';
  anchor.style.height = '100px';
  anchor.style.position = 'relative';
  anchor.innerText = 'Test anchor';
  nudge.insertAdjacentElement('beforebegin', anchor);

  // Update and show the nudge.
  nudge.content = 'Nudge contents';
  nudge.direction = NudgeDirection.BOTTOM_ENDWARD;
  nudge.anchor = anchor;
  nudge.show();

  // The nudge dot should be relative to the anchor.
  assertEquals(nudge.dotRect.y, 112, 'dot y-ordinate');
  assertEquals(nudge.dotRect.x, 54, 'dot x-ordinate');

  // Update the anchor to be 300px inset in the page.
  anchor.style.top = '300px';
  anchor.style.left = '300px';

  // Reposition the nudge should update it's position to be relative to the
  // newly positioned anchor element.
  nudge.reposition();
  assertEquals(nudge.dotRect.y, 412, 'dot y-ordinate');
  assertEquals(nudge.dotRect.x, 354, 'dot x-ordinate');
}

/**
 * Tests that setting the dismissText displays the dismiss button and setting an
 * empty text hides the button.
 */
export async function testDismissButtonHideAndShow() {
  const nudge = getNudge();

  // Create an anchor element and insert it before the nudge in the DOM.
  const anchor = document.createElement('div');
  anchor.style.width = '100px';
  anchor.style.height = '100px';
  anchor.innerText = 'Test anchor';
  nudge.insertAdjacentElement('beforebegin', anchor);

  // Add the dismiss text and display the nudge.
  nudge.content = 'Nudge contents';
  nudge.dismissText = 'Dismiss';
  nudge.direction = NudgeDirection.BOTTOM_ENDWARD;
  nudge.anchor = anchor;
  nudge.show();

  // Check that the button is visible.
  const dismissButton = nudge.shadowRoot!.getElementById('dismiss')!;
  assertEquals(
      dismissButton.innerText, 'Dismiss',
      'dismiss button should show the dismissText');
  assertTrue(dismissButton.getBoundingClientRect().width > 0);

  // <xf-nudge> fires its DISMISS event when user clicks on the dismiss button.
  let clicked = false;
  nudge.addEventListener(XfNudge.events.DISMISS, () => clicked = true);
  dismissButton.click();
  assertTrue(clicked, '<xf-nudge> should fire DISMISS event');

  nudge.hide();

  // Displaying without dismiss text, the button should be hidden.
  nudge.dismissText = '';
  nudge.show();
  assertEquals(
      dismissButton.innerText, '', 'dismiss button text should be empty');
  assertEquals(dismissButton.getBoundingClientRect().width, 0);
}

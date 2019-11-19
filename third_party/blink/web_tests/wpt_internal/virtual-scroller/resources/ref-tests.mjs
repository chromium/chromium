/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Runs a ref-test based on the filename.
 * @package
 */

import * as helpers from './helpers.mjs';

const LESS_THAN_SCREENFUL = 5;
const MORE_THAN_SCREENFUL = 100;
const WORDS_IN_PARAGRAPH = 50;
const FRAMES_TO_SETTLE = 10;

function scrollerSettled() {
  return helpers.nFrames(FRAMES_TO_SETTLE);
}

export function testFull(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
}

export async function testFullScroll500px(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  await scrollerSettled();
  window.scrollBy(0, 500);  // eslint-disable-line no-magic-numbers
};

/**
 * Tests scrolling in 2 steps to the end of the page.
 * This reproduces crbug.com/1004102.
 */
export async function testFullScrollToEndIn2Steps(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  await scrollerSettled();
  target.children[1].scrollIntoView(/* alignToTop= */ true);
  await scrollerSettled();
  window.scrollBy(0, target.getBoundingClientRect().height);
};

export async function testLargeChild(target) {
  const largeChild = helpers.largeDiv('largeChild');
  target.appendChild(largeChild);
  const child = helpers.div('child');
  target.appendChild(child);

  await scrollerSettled();
  window.scrollBy(0, largeChild.getBoundingClientRect().height);
}

export async function testLargeChildComment(target) {
  const largeChild = helpers.largeDiv('largeChild');
  target.appendChild(largeChild);
  // Ensure that non-element nodes don't cause problems.
  target.appendChild(document.createComment('comment'));
  target.appendChild(helpers.div('child'));

  await scrollerSettled();
  window.scrollBy(0, largeChild.getBoundingClientRect().height);
}

export async function testMoveElement(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  await scrollerSettled();
  const element = target.firstElementChild;
  target.appendChild(element);
}

export async function testMoveElementScrollIntoView(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  const element = target.firstElementChild;
  target.appendChild(element);
  await scrollerSettled();
  element.scrollIntoView();
}

export async function testPart(target) {
  helpers.appendDivs(target, LESS_THAN_SCREENFUL, '10px');
}

export async function testResize(target) {
  target.style.overflow = 'hidden';
  target.style.width = '500px';
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  const text = helpers.words(WORDS_IN_PARAGRAPH);
  for (const e of target.children) {
    e.innerText = text;
  }
  await scrollerSettled();
  target.style.width = '300px';
}

export async function testScrollFromOffScreen(target) {
  // The page is a large element (much bigger than the page)
  // followed by the scroller. We then scroll down to the scroller.
  const largeSibling = helpers.largeDiv('large');
  target.before(largeSibling);
  const child = helpers.div('child');
  target.appendChild(child);

  await scrollerSettled();
  window.scrollBy(0, largeSibling.getBoundingClientRect().height);
}

/**
 * Make sure that an element that was hidden by the scroller does not
 * remain hidden if it is moved out of the scroller.
 */
export async function testUnlockAfterRemove(target) {
  helpers.appendDivs(target, MORE_THAN_SCREENFUL, '10px');
  await scrollerSettled();
  const e = target.lastElementChild;
  // Make sure the element can stay locked outside of the scroller.
  e.style.contain = 'style layout';
  target.parentElement.appendChild(e);

  await scrollerSettled();
  window.scrollBy(0, target.getBoundingClientRect().height);
};

/**
 * Runs |test| with a <virtual-scroller>, waiting until the custom element is
 * defined.
 */
export async function withVirtualScroller(test) {
  customElements.whenDefined('virtual-scroller').then(async () => {
    runTest('virtual-scroller', test);
    await scrollerSettled();
    helpers.stopWaiting();
  });
}

/**
 * Runs |test| with a <div>.
 */
export async function withDiv(test) {
  runTest('div', test);
  helpers.stopWaiting();
}

async function runTest(elementName, test) {
  // This scrollTo and await are not necessary for the ref-tests
  // however it helps when trying to debug these tests in a
  // browser. Without it, the scroll offset may be preserved across
  // page reloads.
  document.body.scrollTo(0, 0);
  await helpers.nFrames(1);

  const element = document.createElement(elementName);
  document.body.appendChild(element);
  test(element);
}

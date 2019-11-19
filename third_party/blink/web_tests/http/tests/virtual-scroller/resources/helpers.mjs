/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Helpers for testing virtual-scroller code.
 * @package
 */

import * as wptHelpers from '/wpt_internal/virtual-scroller/resources/helpers.mjs';

import * as findElement from '/gen/third_party/blink/renderer/core/script/resources/layered_api/elements/virtual-scroller/find-element.mjs';

export function topOf(element) {
  return element.getBoundingClientRect().top;
}

export function bottomOf(element) {
  return element.getBoundingClientRect().bottom;
}

/**
 * An inefficient but simple implementation of findElement that searches
 * linearly.
 */
export function simpleFindElement(elements, offset, bias) {
  if (elements.length === 0) {
    return null;
  }
  const lastIndex = elements.length - 1;

  if (bias === findElement.BIAS_LOW) {
    if (offset < topOf(elements[0])) {
      // Lower than first element.
      return null;
    }
    // We bias low, so we accept the first element that either
    // contains offset or is followed by an element that is fully
    // higher than offset.
    for (let i = 0; i < elements.length; i++) {
      const element = elements[i];
      // We already know that offset >= top.
      if (offset <= bottomOf(element) ||
          (i < lastIndex && offset < topOf(elements[i + 1]))) {
        return element;
      }
    }
  } else {
    if (offset > bottomOf(elements[lastIndex])) {
      // Higher than last element.
      return null;
    }
    // We bias high, so we iterate backwards and accept the first
    // element that either contains offset or is followed by an
    // element that is fully lower than offset.
    for (let i = lastIndex; i >= 0; i--) {
      const element = elements[i];
      // We already know that offset <= bottom.
      if (offset >= topOf(element) ||
          (i > 0 && offset > bottomOf(elements[i - 1]))) {
        return element;
      }
    }
  }
  // We went through all the elements without accepting any.
  return null;
}

/**
 * Asserts that we find |element| when we search in |elements| with this
 * |offset| and |bias|.
 */
export function assertFindsElement(elements, offset, bias, element) {
  const found = findElement.findElement(elements, offset, bias);
  window.assert_equals(
      found, element, `Searching offset=${offset}, bias=${bias.toString()}`);
}

/**
 * Starts 10px before the first element and iterates until 10px after the last
 * element.  At each point compares the results of simpleFindElement with the
 * real findElement.
 */
function testFindElementAtAllOffsets(elements, margin, bias) {
  const startPx = topOf(elements[0]);
  const endPx = bottomOf(elements[elements.length - 1]);
  const BUFFER_PX = 10;
  // index will go from -1 to length (both are out of bounds)
  for (let offset = startPx - BUFFER_PX;
      offset <= endPx + BUFFER_PX;
      offset++) {
    const element = simpleFindElement(elements, offset, bias);
    assertFindsElement(elements, offset, bias, element);
  }
}

/**
 * Constructs a container with |elementCount| divs each with margin |margin| and
 * exhaustively tests findElement against this.
 */
export function testFindElement({elementCount, margin}) {
  window.test(() => {
    wptHelpers.withElement('div', containerDiv => {
      wptHelpers.appendDivs(containerDiv, elementCount, margin);
      const elements = containerDiv.children;
      testFindElementAtAllOffsets(elements, margin, findElement.BIAS_LOW);
      testFindElementAtAllOffsets(elements, margin, findElement.BIAS_HIGH);
    });
  }, `Test findElement elementCount=${elementCount}, margin=${margin}`);
}

/**
 * Asserts that the elements of actual and expected are the same, ignoring
 * order. This uses Array#sort() so will only work for elements that can be
 * sorted.
*/
export function assertElementsEqual(actual, expected, description) {
  const actualArray = Array.from(actual);
  actualArray.sort();
  const expectedArray = Array.from(expected);
  expectedArray.sort();
  window.assert_array_equals(actualArray, expectedArray, description);
}


// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertLT} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfSplitter} from './xf_splitter.js';

/**
 * Creates new <xf-splitter> element for each test.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <style>div{width:100%;}</style>
    <xf-splitter>
      <div slot=splitter-before>B<hr/></div>
      <div slot=splitter-after>A<hr/></div>
    </xf-splitter>
  `;
}

/**
 * Returns the <xf-splitter> element.
 */
function getSplitterElement(): XfSplitter {
  return document.querySelector<XfSplitter>('xf-splitter')!;
}

/**
 * Returns the <div> element that is the splitter separator.
 */
function getSplitterDivElement(): HTMLDivElement {
  const splitter = getSplitterElement();
  return splitter.shadowRoot!.querySelector<HTMLDivElement>('#splitter')!;
}

function simulateMouseDown(element: HTMLElement, position: number) {
  const ev = new MouseEvent('mousedown', {
    view: window,
    bubbles: true,
    cancelable: true,
    clientX: position,
  });

  element.dispatchEvent(ev);
}

function simulateMouseMove(element: HTMLElement, position: number) {
  const ev = new MouseEvent('mousemove', {
    view: window,
    bubbles: true,
    cancelable: true,
    clientX: position,
  });

  element.dispatchEvent(ev);
}

function simulateMouseUp(element: HTMLElement) {
  const ev = new MouseEvent('mouseup', {
    view: window,
    bubbles: true,
    cancelable: true,
  });

  element.dispatchEvent(ev);
}

function simulateTouchStart(element: HTMLElement, position: number) {
  const touch = new Touch({clientX: position, identifier: 0, target: element});
  const ev = new TouchEvent('touchstart', {
    view: window,
    bubbles: true,
    cancelable: true,
    touches: [touch],
  });

  element.dispatchEvent(ev);
}

function simulateTouchMove(element: HTMLElement, position: number) {
  const touch = new Touch({clientX: position, identifier: 0, target: element});
  const ev = new TouchEvent('touchmove', {
    view: window,
    bubbles: true,
    cancelable: true,
    touches: [touch],
  });

  element.dispatchEvent(ev);
}

function simulateTouchEnd(element: HTMLElement) {
  const ev = new TouchEvent('touchend', {
    view: window,
    bubbles: true,
    cancelable: true,
  });

  element.dispatchEvent(ev);
}

/**
 * Tests that the default layout takes up the full client width.
 */
export async function testDefaultLayout() {
  const splitter = getSplitterElement();
  await waitForElementUpdate(splitter);
  const splitterWidth = splitter.getBoundingClientRect().width;
  const before = document.querySelector('[slot=splitter-before]')!;
  const beforeWidth = before.getBoundingClientRect().width;
  assertLT(beforeWidth, splitterWidth);
  const after = document.querySelector('[slot=splitter-after]')!;
  const afterWidth = after.getBoundingClientRect().width;
  assertLT(afterWidth, splitterWidth);
  // Check: width of splitter is the two panes plus default splitter bar size.
  assertEquals(
      splitterWidth, beforeWidth + afterWidth + XfSplitter.splitterBarSize);
}

/**
 * Tests that firing move events on the splitter change the child sizes.
 */
export async function testSplitterMove() {
  const splitter = getSplitterElement();
  await waitForElementUpdate(splitter);
  const before = document.querySelector('[slot=splitter-before]')!;
  const beforeWidth = before.getBoundingClientRect().width;
  const after = document.querySelector('[slot=splitter-after]')!;
  const afterWidth = after.getBoundingClientRect().width;
  // Move the splitter 10px to the right with mouse events.
  const splitterDiv = getSplitterDivElement();
  simulateMouseDown(splitterDiv, 0);
  simulateMouseMove(splitter, 10);
  simulateMouseUp(splitter);
  // Check: 'before' section has grown and 'after' section has shrunk.
  assertEquals(beforeWidth + 10, before.getBoundingClientRect().width);
  assertEquals(afterWidth - 10, after.getBoundingClientRect().width);
  // Move the splitter 15px to the right with touch events.
  simulateTouchStart(splitterDiv, 0);
  simulateTouchMove(splitter, 15);
  simulateTouchEnd(splitter);
  // Check: 'before' section has grown and 'after' section has shrunk.
  assertEquals(beforeWidth + 25, before.getBoundingClientRect().width);
  assertEquals(afterWidth - 25, after.getBoundingClientRect().width);
  // Move splitter 12px to the left with mouse events.
  simulateMouseDown(splitterDiv, 0);
  simulateMouseMove(splitter, -12);
  simulateMouseUp(splitter);
  // Check: 'before' section has shrunk and 'after' section has grown.
  assertEquals(beforeWidth + 13, before.getBoundingClientRect().width);
  assertEquals(afterWidth - 13, after.getBoundingClientRect().width);
}

/**
 * Tests that RTL layout changes the child sizes correctly.
 */
export async function testSplitterRTLMove() {
  const splitter = getSplitterElement();
  splitter.setAttribute('dir', 'rtl');
  await waitForElementUpdate(splitter);
  const before = document.querySelector('[slot=splitter-before]')!;
  const beforeWidth = before.getBoundingClientRect().width;
  const after = document.querySelector('[slot=splitter-after]')!;
  const afterWidth = after.getBoundingClientRect().width;
  // Move the splitter 10px to the right with mouse events.
  const splitterDiv = getSplitterDivElement();
  simulateMouseDown(splitterDiv, 0);
  simulateMouseMove(splitter, 10);
  simulateMouseUp(splitter);
  // Check: 'before' section has shrunk and 'after' section has grown.
  assertEquals(beforeWidth - 10, before.getBoundingClientRect().width);
  assertEquals(afterWidth + 10, after.getBoundingClientRect().width);
  // Move the splitter 15px to the right with touch events.
  simulateTouchStart(splitterDiv, 0);
  simulateTouchMove(splitter, 15);
  simulateTouchEnd(splitter);
  // Check: 'before' section has shrunk and 'after' section has grown.
  assertEquals(beforeWidth - 25, before.getBoundingClientRect().width);
  assertEquals(afterWidth + 25, after.getBoundingClientRect().width);
  // Move splitter 12px to the left with mouse events.
  simulateMouseDown(splitterDiv, 0);
  simulateMouseMove(splitter, -12);
  simulateMouseUp(splitter);
  // Check: 'before' section has grown and 'after' section has shrunk.
  assertEquals(beforeWidth - 13, before.getBoundingClientRect().width);
  assertEquals(afterWidth + 13, after.getBoundingClientRect().width);
}

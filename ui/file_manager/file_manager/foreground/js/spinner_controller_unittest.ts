// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {SpinnerController} from './spinner_controller.js';

let spinner: HTMLElement;
let controller: SpinnerController;

function waitForMutation(target: HTMLElement) {
  return new Promise<void>((fulfill: VoidCallback) => {
    const observer = new MutationObserver(() => {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

export function setUp() {
  spinner = document.createElement('div');
  spinner.id = 'spinner';
  spinner.textContent = 'LOADING...';
  spinner.hidden = true;
  controller = new SpinnerController(spinner);
  // Set the duration to 100ms, which is short enough, but also long enough
  // to happen later than 0ms timers used in test cases.
  controller.setBlinkDurationForTesting(100);
}

export async function testBlink() {
  assertTrue(spinner.hidden);
  controller.blink();

  await waitForMutation(spinner);
  assertFalse(spinner.hidden);

  await waitForMutation(spinner);
  assertTrue(spinner.hidden);
}

export async function testShow() {
  assertTrue(spinner.hidden);
  const hideCallback = controller.show();

  await waitForMutation(spinner);
  assertFalse(spinner.hidden);

  await new Promise<void>((fulfill: VoidCallback) => {
    setTimeout(fulfill, 0);
  });
  assertFalse(spinner.hidden);  // It should still be hidden.
  // Call asynchronously, so the mutation observer catches the change.
  setTimeout(hideCallback, 0);
  await waitForMutation(spinner);
  assertTrue(spinner.hidden);
}

export async function testShowDuringBlink() {
  assertTrue(spinner.hidden);
  controller.blink();
  const hideCallback = controller.show();

  await waitForMutation(spinner);
  assertFalse(spinner.hidden);

  await new Promise<void>((fulfill: VoidCallback) => {
    setTimeout(fulfill, 0);
  });
  assertFalse(spinner.hidden);
  hideCallback();
  await new Promise<void>((fulfill: VoidCallback) => {
    setTimeout(fulfill, 0);
  });
  assertFalse(spinner.hidden);
  await waitForMutation(spinner);

  assertTrue(spinner.hidden);
}

export async function testStackedShows() {
  assertTrue(spinner.hidden);

  const hideCallbacks: VoidCallback[] = [];
  hideCallbacks.push(controller.show());
  hideCallbacks.push(controller.show());

  await waitForMutation(spinner);
  assertFalse(spinner.hidden);

  await new Promise<void>((fulfill: VoidCallback) => {
    setTimeout(fulfill, 0);
  });
  assertFalse(spinner.hidden);
  hideCallbacks[1]!();

  await new Promise<void>((fulfill: VoidCallback) => {
    setTimeout(fulfill, 0);
  });
  assertFalse(spinner.hidden);

  setTimeout(hideCallbacks[0]!, 0);
  await waitForMutation(spinner);
  assertTrue(spinner.hidden);
}

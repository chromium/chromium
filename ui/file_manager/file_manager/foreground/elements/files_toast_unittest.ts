// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../../common/js/test_error_reporting.js';

import {FilesToast} from './files_toast.js';

export function setUpPage() {
  const filesToastElement = document.createElement('files-toast');
  document.body.append(filesToastElement);
}

export async function testToast(done: VoidCallback) {
  const toast = document.querySelector<FilesToast>('files-toast')!;
  const text = toast.shadowRoot!.querySelector<HTMLDivElement>('#text')!;
  const action = toast.shadowRoot!.querySelector<CrButtonElement>('#action')!;
  const getToastOpacity = () => {
    return parseFloat(
        window
            .getComputedStyle(
                toast.shadowRoot!.querySelector<CrToastElement>('cr-toast')!)
            .opacity);
  };

  // Toast is hidden to start.
  assertFalse(toast.visible);

  // Show toast1, wait for cr-toast to finish animating, and then verify all the
  // properties and HTML is correct.
  let a1Called = false;
  toast.show('t1', {
    text: 'a1',
    callback: () => {
      a1Called = true;
    },
  });
  await waitUntil(() => getToastOpacity() === 1);
  assertTrue(toast.visible);
  assertEquals('t1', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a1', action.innerText);

  // Queue up toast2 and toast3, should still be showing toast1.
  let a2Called = false;
  toast.show('t2', {
    text: 'a2',
    callback: () => {
      a2Called = true;
    },
  });
  toast.show('t3');
  assertEquals('t1', text.innerText);

  // Invoke toast1 action, callback will be called.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a1Called);

  // Wait for toast1 to finish hiding and then wait for toast2 to finish
  // showing.
  await waitUntil(() => getToastOpacity() === 0);
  await waitUntil(() => getToastOpacity() === 1);

  assertTrue(toast.visible);
  assertEquals('t2', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a2', action.innerText);

  // Invoke toast2 action, callback will be called.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a2Called);

  // Wait for toast2 to finish hiding and wait for toast3 to finish showing.
  await waitUntil(() => getToastOpacity() === 0);
  await waitUntil(() => getToastOpacity() === 1);

  assertTrue(toast.visible);
  assertEquals('t3', text.innerText);
  assertTrue(action.hidden);

  // Call hide(), toast should no longer be visible, no more toasts shown.
  toast.hide();
  await waitUntil(() => getToastOpacity() === 0);
  await waitUntil(() => !toast.visible);

  done();
}

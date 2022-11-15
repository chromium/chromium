// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_toast.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

export function setUpPage() {
  document.body.innerHTML += '<files-toast></files-toast>';
}

export async function testToast(done) {
  /** @type {FilesToast|Element} */
  const toast = document.querySelector('files-toast');
  const text = toast.shadowRoot.querySelector('#text');
  const action = toast.shadowRoot.querySelector('#action');
  const waitFor = async f => {
    while (!f()) {
      await new Promise(r => setTimeout(r, 0));
    }
  };
  const getToastOpacity = () => {
    return parseFloat(
        window.getComputedStyle(toast.shadowRoot.querySelector('cr-toast'))
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
  await waitFor(() => getToastOpacity() === 1);
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
  await waitFor(() => getToastOpacity() === 0);
  await waitFor(() => getToastOpacity() === 1);

  assertTrue(toast.visible);
  assertEquals('t2', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a2', action.innerText);

  // Invoke toast2 action, callback will be called.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a2Called);

  // Wait for toast2 to finish hiding and wait for toast3 to finish showing.
  await waitFor(() => getToastOpacity() === 0);
  await waitFor(() => getToastOpacity() === 1);

  assertTrue(toast.visible);
  assertEquals('t3', text.innerText);
  assertTrue(action.hidden);

  // Call hide(), toast should no longer be visible, no more toasts shown.
  toast.hide();
  await waitFor(() => getToastOpacity() === 0);
  await waitFor(() => !toast.visible);

  done();
}

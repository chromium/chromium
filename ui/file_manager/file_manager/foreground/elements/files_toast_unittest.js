// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setUpPage() {
  document.body.innerHTML += '<files-toast></files-toast>';
}

async function testToast(done) {
  /** @type {FilesToast|Element} */
  const toast = document.querySelector('files-toast');
  const text = toast.shadowRoot.querySelector('#text');
  const action = toast.shadowRoot.querySelector('#action');
  const waitFor = async f => {
    while (!f()) {
      await new Promise(r => setTimeout(r, 0));
    }
  };

  // Toast is hidden to start.
  assertFalse(toast.visible);

  // Show toast1, verify visible, text and action text.
  let a1Called = false;
  toast.show('t1', {
    text: 'a1',
    callback: () => {
      a1Called = true;
    }
  });
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
    }
  });
  toast.show('t3');
  assertEquals('t1', text.innerText);

  // Invoke toast1 action, callback will be called,
  // and toast2 will show after animation.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a1Called);
  await waitFor(() => text.innerText === 't2');
  assertTrue(toast.visible);
  assertEquals('t2', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a2', action.innerText);

  // Invoke toast2 action, callback will be called,
  // and toast3 will show after animation with no action.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a2Called);
  await waitFor(() => text.innerText === 't3');
  assertTrue(toast.visible);
  assertEquals('t3', text.innerText);
  assertTrue(action.hidden);

  // Call hide(), toast should no longer be visible, no more toasts shown.
  toast.hide();
  await waitFor(() => !toast.visible);

  done();
}

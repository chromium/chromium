// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testShowDialogAfterHide(done) {
  // Polyfill chrome.app.window.current().
  /** @suppress {duplicate,checkTypes,const} */
  chrome.app = {window: {current: () => null}};

  const container =
      assertInstanceof(document.createElement('div'), HTMLElement);

  function isShown() {
    return !!container.querySelector('.cr-dialog-container.shown');
  }

  const dialog = new FileManagerDialogBase(container);
  // Show the dialog and wait until .shown is set on .cr-dialog-container.
  // This happens async.
  dialog.showBlankDialog();
  await waitUntil(isShown);

  // Call hide, and validate .shown is removed (sync).
  dialog.hide();
  assertFalse(isShown());

  // Show the dialog again and ensure that it gets displayed.
  // Previously some async processing from hide() would stop
  // the dialog showing again at all if it was called too soon.
  dialog.showBlankDialog();
  await waitUntil(isShown);
  done();
}

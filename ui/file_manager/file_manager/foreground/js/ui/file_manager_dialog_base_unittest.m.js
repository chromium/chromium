// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {assertFalse} from 'chrome://test/chai_assert.js';
import {waitUntil} from '../../../../base/js/test_error_reporting.m.js';
import {FileManagerDialogBase} from './file_manager_dialog_base.m.js';

export function setUp() {
  // Polyfill chrome.app.window.current().
  /** @suppress {duplicate,checkTypes,const} */
  chrome.app = {window: {current: () => null}};

  window.loadTimeData.data = {
    FILES_NG_ENABLED: true,
  };
}

export async function testShowDialogAfterHide(done) {
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

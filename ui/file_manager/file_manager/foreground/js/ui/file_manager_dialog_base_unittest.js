// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitUntil} from '../../../common/js/test_error_reporting.js';

import {FileManagerDialogBase} from './file_manager_dialog_base.js';

export async function testShowDialogAfterHide(done) {
  const dialog = new FileManagerDialogBase(document.body);

  /** @return {boolean} True if cr.ui.dialog container has .shown class */
  function isShown() {
    const element = document.querySelector('.cr-dialog-container');
    return element.classList.contains('shown');
  }

  // Show the dialog and wait until .shown is set on .cr-dialog-container.
  // The setting of .shown happens async.
  dialog.showBlankDialog();
  await waitUntil(isShown);

  // Hide the dialog and verify .shown is removed (sync).
  dialog.hide();
  assertFalse(isShown());

  // Show the dialog again and ensure that it gets displayed.
  // Previously some async processing from hide() would stop
  // the dialog showing again at all if it was called too soon.
  dialog.showBlankDialog();
  await waitUntil(isShown);

  done();
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface implemented in the foreground page that the background page uses to
 * show dialogs originating from Drive to the user.
 * @interface
 */
export class DriveDialogControllerInterface {
  /**
   * @return {boolean} Whether the dialog is open or not
   */
  get open() {}

  /**
   * Sets up the given dialog for a given Drive dialog type, and shows it.
   * @param {!chrome.fileManagerPrivate.DriveConfirmDialogEvent} event
   */
  showDialog(event) {}
}

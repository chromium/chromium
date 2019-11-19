// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class is an extended class, to manage the status of the dialogs.
 */
class FileManagerDialogBase extends cr.ui.dialogs.BaseDialog {
  /**
   * @param {HTMLElement} parentNode Parent node of the dialog.
   */
  constructor(parentNode) {
    super(parentNode);
  }

  /**
   * @param {string} title Title.
   * @param {string} message Message.
   * @param {?function()} onOk Called when the OK button is pressed.
   * @param {?function()} onCancel Called when the cancel button is pressed.
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   */
  showOkCancelDialog(title, message, onOk, onCancel) {
    return this.showImpl_(title, message, onOk, onCancel);
  }

  /**
   * @param {string} title Title.
   * @param {string} message Message.
   * @param {?function()} onOk Called when the OK button is pressed.
   * @param {?function()} onCancel Called when the cancel button is pressed.
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   * @private
   */
  showImpl_(title, message, onOk, onCancel) {
    if (FileManagerDialogBase.shown) {
      return false;
    }

    FileManagerDialogBase.shown = true;

    // If a dialog is shown, activate the window.
    const appWindow = chrome.app.window.current();
    if (appWindow) {
      appWindow.focus();
    }

    super.showWithTitle(title, message, onOk, onCancel, null);

    return true;
  }

  /**
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   */
  showBlankDialog() {
    return this.showImpl_('', '', null, null);
  }

  /**
   * @param {string} title Title.
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   */
  showTitleOnlyDialog(title) {
    return this.showImpl_(title, '', null, null);
  }

  /**
   * @param {string} title Title.
   * @param {string} text Text to be shown in the dialog.
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   */
  showTitleAndTextDialog(title, text) {
    this.buttons.style.display = 'none';
    return this.showImpl_(title, text, null, null);
  }

  /**
   * @param {Function=} opt_onHide Called when the dialog is hidden.
   */
  hide(opt_onHide) {
    FileManagerDialogBase.shown = false;
    super.hide(() => {
      if (opt_onHide) {
        opt_onHide();
      }
    });
  }
}

/**
 * The flag if any dialog is shown. True if a dialog is visible, false
 *     otherwise.
 * @type {boolean}
 */
FileManagerDialogBase.shown = false;

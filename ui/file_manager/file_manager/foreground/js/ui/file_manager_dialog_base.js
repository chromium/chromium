// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../../common/js/util.js';

import {BaseDialog} from './dialogs.js';

/**
 * This class is an extended class, to manage the status of the dialogs.
 */
export class FileManagerDialogBase extends BaseDialog {
  /**
   * @param {HTMLElement} parentNode Parent node of the dialog.
   */
  constructor(parentNode) {
    super(parentNode);

    this.container.classList.add('files-ng');
  }

  /**
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    super.hasModalContainer = true;
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
    window.focus();

    super.showWithTitle(title, message, onOk, onCancel, null);

    return true;
  }

  /**
   * @override
   */
  showWithTitle(title, message, ...args) {
    this.frame.classList.toggle('no-title', !title);
    super.showWithTitle(title, message, ...args);
  }

  /**
   * @override
   */
  showHtml(title, message, ...args) {
    this.frame.classList.toggle('no-title', !title);
    super.showHtml(title, message, ...args);
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
   * @override
   * @suppress {accessControls}
   */
  show_(...args) {
    this.parentNode_ = util.getFilesAppModalDialogInstance();

    super.show_(...args);

    this.parentNode_.showModal();
  }

  /**
   * @override
   */
  hide(...args) {
    this.parentNode_.close();

    FileManagerDialogBase.shown = false;
    super.hide(...args);
  }
}

/**
 * The flag if any dialog is shown. True if a dialog is visible, false
 *     otherwise.
 * @type {boolean}
 */
FileManagerDialogBase.shown = false;

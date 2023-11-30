// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilesAppModalDialogInstance} from '../../../common/js/util.js';

import {BaseDialog} from './dialogs.js';

/**
 * This class is an extended class, to manage the status of the dialogs.
 */
// @ts-ignore: error TS2415: Class 'FileManagerDialogBase' incorrectly extends
// base class 'BaseDialog'.
export class FileManagerDialogBase extends BaseDialog {
  /**
   * @param {HTMLElement} parentNode Parent node of the dialog.
   */
  constructor(parentNode) {
    super(parentNode);

    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
   * @param {?function():void} onOk Called when the OK button is pressed.
   * @param {?function():void} onCancel Called when the cancel button is
   *     pressed.
   * @return {boolean} True if the dialog can show successfully. False if the
   *     dialog failed to show due to an existing dialog.
   */
  showOkCancelDialog(title, message, onOk, onCancel) {
    return this.showImpl_(title, message, onOk, onCancel);
  }

  /**
   * @param {string} title Title.
   * @param {string} message Message.
   * @param {?function():void} onOk Called when the OK button is pressed.
   * @param {?function():void} onCancel Called when the cancel button is
pressed.
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

    // @ts-ignore: error TS2345: Argument of type '(() => any) | null' is not
    // assignable to parameter of type 'Function | undefined'.
    super.showWithTitle(title, message, onOk, onCancel, null);

    return true;
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  showWithTitle(title, message, ...args) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.classList.toggle('no-title', !title);
    super.showWithTitle(title, message, ...args);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  showHtml(title, message, ...args) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.buttons.style.display = 'none';
    return this.showImpl_(title, text, null, null);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  show_(...args) {
    this.parentNode_ = getFilesAppModalDialogInstance();

    // @ts-ignore: error TS2556: A spread argument must either have a tuple type
    // or be passed to a rest parameter.
    super.show_(...args);

    this.parentNode_.showModal();
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
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

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilesAppModalDialogInstance} from '../../../common/js/util.js';

import {ConfirmDialog} from './dialogs.js';

/**
 * Confirm dialog.
 */
// @ts-ignore: error TS2415: Class 'FilesConfirmDialog' incorrectly extends base
// class 'ConfirmDialog'.
export class FilesConfirmDialog extends ConfirmDialog {
  /**
   * @param {!Element} parentElement
   */
  constructor(parentElement) {
    super(parentElement);

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.container.classList.add('files-ng');

    /**
     * @type {?function():void} showModalElement Optional call to show the
     * modal <dialog> parent of |this| if needed.
     * @public
     */
    this.showModalElement = null;

    /**
     * @type {?function():void} doneCallback Optional callback when |this|
     * is closed confirmed or cancelled via dialog buttons.
     * @public
     */
    this.doneCallback = null;

    /**
     * @type {boolean} focusCancelButton Set true if the cancel button
     * should be focused when the dialog is first displayed. Otherwise
     * (the default) the dialog will focus the confirm button.
     * @public
     */
    this.focusCancelButton = false;
  }

  /**
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    super.hasModalContainer = true;

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.classList.add('files-confirm-dialog');
  }

  /**
   * @override
   * @suppress {accessControls}
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  show_(...args) {
    if (!this.showModalElement) {
      this.parentNode_ = getFilesAppModalDialogInstance();
    }

    if (this.focusCancelButton) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.frame.classList.add('files-confirm-dialog-cancel-default');
      this.setInitialFocusOnCancel();
    }

    // @ts-ignore: error TS2556: A spread argument must either have a tuple type
    // or be passed to a rest parameter.
    super.show_(...args);

    if (!this.showModalElement) {
      this.parentNode_.showModal();
    }
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7019: Rest parameter 'args' implicitly has an 'any[]'
  // type.
  hide(...args) {
    if (!this.showModalElement) {
      this.parentNode_.close();
    }

    super.hide(...args);
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
}

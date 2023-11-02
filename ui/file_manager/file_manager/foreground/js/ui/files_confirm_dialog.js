// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../../common/js/util.js';

import {ConfirmDialog} from './dialogs.js';

/**
 * Confirm dialog.
 */
export class FilesConfirmDialog extends ConfirmDialog {
  /**
   * @param {!Element} parentElement
   */
  constructor(parentElement) {
    super(parentElement);

    this.container.classList.add('files-ng');

    /**
     * @type {?function()} showModalElement Optional call to show the
     * modal <dialog> parent of |this| if needed.
     * @public
     */
    this.showModalElement = null;

    /**
     * @type {?function()} doneCallback Optional callback when |this|
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

    this.frame.classList.add('files-confirm-dialog');
  }

  /**
   * @override
   * @suppress {accessControls}
   */
  show_(...args) {
    if (!this.showModalElement) {
      this.parentNode_ = util.getFilesAppModalDialogInstance();
    }

    if (this.focusCancelButton) {
      this.frame.classList.add('files-confirm-dialog-cancel-default');
      this.setInitialFocusOnCancel();
    }

    super.show_(...args);

    if (!this.showModalElement) {
      this.parentNode_.showModal();
    }
  }

  /**
   * @override
   */
  hide(...args) {
    if (!this.showModalElement) {
      this.parentNode_.close();
    }

    super.hide(...args);
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
}

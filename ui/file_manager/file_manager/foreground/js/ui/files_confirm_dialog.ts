// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilesAppModalDialogInstance} from '../../../common/js/util.js';

import {ConfirmDialog} from './dialogs.js';

/**
 * Confirm dialog.
 */
export class FilesConfirmDialog extends ConfirmDialog {
  /**
   * showModalElement Optional call to show the
   * modal <dialog> parent of |this| if needed.
   */
  showModalElement: VoidCallback|null = null;

  /**
   * doneCallback Optional callback when |this|
   * is closed confirmed or cancelled via dialog buttons.
   */
  doneCallback: VoidCallback|null = null;

  /**
   * focusCancelButton Set true if the cancel button
   * should be focused when the dialog is first displayed. Otherwise
   * (the default) the dialog will focus the confirm button.
   */
  focusCancelButton: boolean = false;

  /**
   */
  constructor(parentElement: HTMLElement) {
    super(parentElement);

    this.container.classList.add('files-ng');
  }

  override initDom() {
    super.initDom();
    this.hasModalContainer = true;

    this.frame.classList.add('files-confirm-dialog');
  }

  protected override show_(
      title: string, onOk?: VoidCallback, onCancel?: VoidCallback,
      onShow?: VoidCallback) {
    if (!this.showModalElement) {
      this.parentNode_ = getFilesAppModalDialogInstance();
    }

    if (this.focusCancelButton) {
      this.frame.classList.add('files-confirm-dialog-cancel-default');
      this.setInitialFocusOnCancel();
    }

    super.show_(title, onOk, onCancel, onShow);

    if (!this.showModalElement) {
      this.parentNodeAsDialogTag.showModal();
    }
  }

  override hide(onHide?: VoidCallback) {
    super.hide(onHide);
    if (!this.showModalElement) {
      this.parentNodeAsDialogTag.close();
    }
  }

  get parentNodeAsDialogTag(): HTMLDialogElement {
    // Before calling this, it's expected that this.parentNode_ was assigned to
    // a <dialog> element.
    return this.parentNode_ as HTMLDialogElement;
  }

  override showWithTitle(
      title: string, message: string, onOk?: VoidCallback,
      onCancel?: VoidCallback, onShow?: VoidCallback) {
    this.frame.classList.toggle('no-title', !title);
    super.showWithTitle(title, message, onOk, onCancel, onShow);
  }

  override showHtml(
      title: string, message: string, onOk?: VoidCallback,
      onCancel?: VoidCallback, onShow?: VoidCallback) {
    this.frame.classList.toggle('no-title', !title);
    super.showHtml(title, message, onOk, onCancel, onShow);
  }
}

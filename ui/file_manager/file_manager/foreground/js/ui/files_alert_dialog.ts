// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilesAppModalDialogInstance} from '../../../common/js/util.js';

import {AlertDialog} from './dialogs.js';

/**
 * Alert dialog.
 */
export class FilesAlertDialog extends AlertDialog {
  /**
   */
  constructor(parentNode: HTMLElement) {
    super(parentNode);

    this.container.classList.add('files-ng');
  }

  override initDom() {
    super.initDom();
    this.hasModalContainer = true;

    this.frame.classList.add('files-alert-dialog');
  }

  get parentNode(): HTMLDialogElement {
    this.parentNode_ = getFilesAppModalDialogInstance();
    return this.parentNode_ as HTMLDialogElement;
  }

  protected override show_(
      title: string, onOk?: VoidCallback, onCancel?: VoidCallback,
      onShow?: VoidCallback) {
    this.parentNode_ = getFilesAppModalDialogInstance();

    super.show_(title, onOk, onCancel, onShow);

    this.parentNode.showModal();
  }

  override hide(onHide?: VoidCallback) {
    super.hide(onHide);
    this.parentNode.close();
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

  /**
   * Async version of show(). Resolves when the alert dialog is dismissed.
   */
  showAsync(title: string): Promise<void> {
    return new Promise(resolve => this.show(title, resolve));
  }
}

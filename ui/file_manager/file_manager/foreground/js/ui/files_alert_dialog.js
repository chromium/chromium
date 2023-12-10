// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilesAppModalDialogInstance} from '../../../common/js/util.js';

import {AlertDialog} from './dialogs.js';

/**
 * Alert dialog.
 */
// @ts-ignore: error TS2415: Class 'FilesAlertDialog' incorrectly extends base
// class 'AlertDialog'.
export class FilesAlertDialog extends AlertDialog {
  /**
   * @param {!HTMLElement} parentNode
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

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.classList.add('files-alert-dialog');
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

  /**
   * Async version of show().
   * @param {string} title
   * @returns {!Promise<void>} Resolves when dismissed.
   */
  showAsync(title) {
    return new Promise(resolve => this.show(title, resolve));
  }
}

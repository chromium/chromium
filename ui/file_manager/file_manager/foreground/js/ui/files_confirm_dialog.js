// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {util} from '../../../common/js/util.m.js';
// #import {ConfirmDialog} from 'chrome://resources/js/cr/ui/dialogs.m.js';

/**
 * Confirm dialog.
 */
/* #export */ class FilesConfirmDialog extends cr.ui.dialogs.ConfirmDialog {
  /**
   * @param {!Element} parentElement
   */
  constructor(parentElement) {
    super(parentElement);

    if (util.isFilesNg()) {
      this.container.classList.add('files-ng');
    }

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
  }

  /**
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    this.frame.classList.add('files-confirm-dialog');
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

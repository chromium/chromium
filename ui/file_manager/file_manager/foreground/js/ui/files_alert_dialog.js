// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Alert dialog.
 */
class FilesAlertDialog extends cr.ui.dialogs.AlertDialog {
  /**
   * @param {!HTMLElement} parentNode
   */
  constructor(parentNode) {
    super(parentNode);
  }

  /**
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    this.frame.classList.add('files-alert-dialog');
  }
}

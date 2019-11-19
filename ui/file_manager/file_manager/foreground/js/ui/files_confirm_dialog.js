// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Confirm dialog.
 */
class FilesConfirmDialog extends cr.ui.dialogs.ConfirmDialog {
  /**
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    this.frame.classList.add('files-confirm-dialog');
  }
}

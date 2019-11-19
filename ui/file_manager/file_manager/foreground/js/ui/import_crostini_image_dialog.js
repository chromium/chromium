// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ImportCrostiniImageDialog is used as the handler for .tini files.
 */
cr.define('cr.filebrowser', () => {
  /**
   * Creates dialog in DOM.
   */
  class ImportCrostiniImageDialog extends cr.ui.dialogs.ConfirmDialog {
    /**
     * @param {HTMLElement} parentNode Node to be parent for this dialog.
     */
    constructor(parentNode) {
      super(parentNode);
      super.setOkLabel(str('IMPORT_CROSTINI_IMAGE_DIALOG_OK_LABEL'));
    }

    /**
     * Shows the dialog.
     *
     * @param {!Entry} entry
     */
    showImportCrostiniImageDialog(entry) {
      super.showWithTitle(
          str('IMPORT_CROSTINI_IMAGE_DIALOG_TITLE'),
          str('IMPORT_CROSTINI_IMAGE_DIALOG_DESCRIPTION'),
          chrome.fileManagerPrivate.importCrostiniImage.bind(null, entry));
    }
  }

  return {ImportCrostiniImageDialog};
});

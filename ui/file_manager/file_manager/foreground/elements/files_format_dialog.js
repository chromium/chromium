// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'files-format-dialog',

  behaviors: [I18nBehavior],

  properties: {
    label_: {
      type: String,
      value: '',
    },

    /** @type {chrome.fileManagerPrivate.FormatFileSystemType} */
    formatType_: {
      type: String,
      value: chrome.fileManagerPrivate.FormatFileSystemType.VFAT,
    },

    space_used_: {
      type: String,
      value: '',
    }
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  format_: async function() {
    try {
      await util.validateExternalDriveName(
          this.label_,
          /** @type {!VolumeManagerCommon.FileSystemType} */
          (this.formatType_));
    } catch (errorMessage) {
      this.$.label.setAttribute('error-message', errorMessage);
      this.$.label.invalid = true;
      return;
    }
    chrome.fileManagerPrivate.formatVolume(
        this.volumeInfo_.volumeId, this.formatType_, this.label_);
    this.$.dialog.close();
  },

  /**
   * Shows the dialog for drive represented by |volumeInfo|.
   * @param {!VolumeInfo} volumeInfo
   */
  showModal: function(volumeInfo) {
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.space_used_ = '';

    this.volumeInfo_ = volumeInfo;
    if (volumeInfo.displayRoot) {
      chrome.fileManagerPrivate.getDirectorySize(
          volumeInfo.displayRoot, space_used_ => {
            if (space_used_ > 0 && volumeInfo === this.volumeInfo_) {
              this.space_used_ = util.bytesToString(space_used_);
            }
            if (window.IN_TEST) {
              this.$['warning-container'].setAttribute('fully-initialized', '');
            }
          });
    }

    this.$.dialog.showModal();
  },
});

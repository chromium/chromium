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
    },

    isErase_: {
      type: Boolean,
      value: false,
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

    if (this.isErase_) {
      chrome.fileManagerPrivate.singlePartitionFormat(
          this.root_.devicePath_, this.formatType_, this.label_);
    } else {
      chrome.fileManagerPrivate.formatVolume(
          this.volumeInfo_.volumeId, this.formatType_, this.label_);
    }
    this.$.dialog.close();
  },

  /**
   * Used to set "single-partition-format" attribute on element.
   * It is used to check flag status in the tests.
   * @return {string}
   *
   * @private
   */
  getSinglePartitionFormat() {
    if (util.isSinglePartitionFormatEnabled()) {
      return 'single-partition-format';
    }
    return '';
  },

  /**
   * @param {!boolean} is_erase
   * @return {string}
   *
   * @private
   */
  getConfirmLabel_: function(is_erase) {
    if (util.isSinglePartitionFormatEnabled()) {
      if (is_erase) {
        return this.i18n('REPARTITION_DIALOG_CONFIRM_LABEL');
      } else {
        return this.i18n('FORMAT_DIALOG_CONFIRM_SHORT_LABEL');
      }
    } else {
      return this.i18n('FORMAT_DIALOG_CONFIRM_LABEL');
    }
  },

  /**
   * @param {!boolean} is_erase
   * @return {string}
   *
   * @private
   */
  getDialogMessage_: function(is_erase) {
    if (util.isSinglePartitionFormatEnabled()) {
      if (is_erase) {
        return this.i18n('REPARTITION_DIALOG_MESSAGE');
      } else {
        return this.i18n('FORMAT_PARTITION_DIALOG_MESSAGE');
      }
    } else {
      return this.i18n('FORMAT_DIALOG_MESSAGE');
    }
  },

  /**
   * Shows the dialog for drive represented by |volumeInfo|.
   * @param {!VolumeInfo} volumeInfo
   */
  showModal: function(volumeInfo) {
    this.isErase_ = false;
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.space_used_ = '';

    this.volumeInfo_ = volumeInfo;
    this.title = this.volumeInfo_.label;
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

  /**
   * Shows the dialog for erasing device.
   * @param {!EntryList} root
   */
  showEraseModal: function(root) {
    this.isErase_ = true;
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.space_used_ = '';

    this.root_ = root;
    this.title = root.label;
    const childVolumes =
        /** @type {Array<VolumeEntry>} */ (this.root_.getUIChildren());
    let totalSpaceUsed = 0;

    const getSpaceUsedRequests = childVolumes.map((childVolume) => {
      return new Promise((resolve) => {
        const volumeInfo = childVolume.volumeInfo;
        if (volumeInfo.displayRoot) {
          chrome.fileManagerPrivate.getDirectorySize(
              volumeInfo.displayRoot, space_used_ => {
                totalSpaceUsed += space_used_;
                if (totalSpaceUsed > 0) {
                  this.space_used_ = util.bytesToString(totalSpaceUsed);
                }
                resolve();
              });
        }
      });
    });

    Promise.all(getSpaceUsedRequests).then(() => {
      if (window.IN_TEST) {
        this.$['warning-container'].setAttribute('fully-initialized', '');
      }
    });
    this.$.dialog.showModal();
  },
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_format_dialog.js

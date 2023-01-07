// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {validateExternalDriveName} from '../js/file_rename.js';

Polymer({
  _template: html`{__html_template__}`,

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
    },
  },

  ready: function() {
    this.$.dialog.consumeKeydownEvent = true;
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  format_: function() {
    try {
      validateExternalDriveName(
          this.label_,
          /** @type {!VolumeManagerCommon.FileSystemType} */
          (this.formatType_));
    } catch (error) {
      this.$.label.setAttribute('error-message', error.message);
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

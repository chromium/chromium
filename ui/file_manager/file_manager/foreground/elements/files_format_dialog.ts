// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {bytesToString, strf} from '../../common/js/translations.js';
import type {FileSystemType} from '../../common/js/volume_manager_types.js';
import {validateExternalDriveName} from '../js/file_rename.js';

import {getTemplate} from './files_format_dialog.html.js';

export interface FilesFormatDialog {
  $: {
    dialog: CrDialogElement,
    label: CrInputElement,
    'warning-container': HTMLElement,
  };
  label_: string;
  formatType_: chrome.fileManagerPrivate.FormatFileSystemType;
  spaceUsed_: string;
}

export class FilesFormatDialog extends PolymerElement {
  static get is() {
    return 'files-format-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label_: {
        type: String,
        value: '',
      },

      formatType_: {
        type: String,
        value: chrome.fileManagerPrivate.FormatFileSystemType.VFAT,
      },

      spaceUsed_: {
        type: String,
        value: '',
      },
    };
  }

  private volumeInfo_: VolumeInfo|null = null;

  override ready() {
    super.ready();

    this.$.dialog.consumeKeydownEvent = true;
  }

  cancel() {
    this.$.dialog.cancel();
  }

  format() {
    try {
      validateExternalDriveName(
          this.label_, this.formatType_ as unknown as FileSystemType);
    } catch (error: any) {
      this.$.label.setAttribute('error-message', error.message);
      this.$.label.invalid = true;
      return;
    }

    chrome.fileManagerPrivate.formatVolume(
        this.volumeInfo_?.volumeId || '', this.formatType_, this.label_);
    this.$.dialog.close();
  }

  getStrf(token: string, value: string): string {
    return strf(token, value);
  }

  /**
   * Shows the dialog for drive represented by |volumeInfo|.
   */
  showModal(volumeInfo: VolumeInfo) {
    this.label_ = '';
    this.formatType_ = chrome.fileManagerPrivate.FormatFileSystemType.VFAT;
    this.spaceUsed_ = '';

    this.volumeInfo_ = volumeInfo;
    this.title = this.volumeInfo_.label;
    if (volumeInfo.displayRoot) {
      chrome.fileManagerPrivate.getDirectorySize(
          volumeInfo.displayRoot, (spaceUsed: number) => {
            if (spaceUsed > 0 && volumeInfo === this.volumeInfo_) {
              this.spaceUsed_ = bytesToString(spaceUsed);
            }
            if (window.IN_TEST) {
              this.$['warning-container'].setAttribute('fully-initialized', '');
            }
          });
    }

    this.$.dialog.showModal();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FilesFormatDialog.is]: FilesFormatDialog;
  }
}

customElements.define(FilesFormatDialog.is, FilesFormatDialog);

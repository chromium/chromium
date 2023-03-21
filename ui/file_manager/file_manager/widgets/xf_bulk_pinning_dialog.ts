// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {str} from '../common/js/util.js';

import {css, customElement, html, query, XfBase} from './xf_base.js';

/**
 * Dialog that shows the benefits of enabling bulk pinning along with storage
 * information if the feature can't be enabled.
 */
@customElement('xf-bulk-pinning-dialog')
export class XfBulkPinningDialog extends XfBase {
  @query('cr-dialog') private $dialog_?: CrDialogElement;

  private copy_ = {
    cancel: str('CANCEL_LABEL'),
    continue: 'Turn on sync',  // TODO: replace with final copy when available.
  };

  show() {
    this.$dialog_!.showModal();
  }

  private onContinue() {
    this.$dialog_!.close();
    chrome.fileManagerPrivate.setPreferences(
        {driveFsBulkPinningEnabled: true} as
        chrome.fileManagerPrivate.PreferencesChange);
  }

  private onCancel() {
    this.$dialog_!.cancel();
  }

  static override get styles() {
    return getCSS();
  }

  override render() {
    return html`
      <cr-dialog>
        <div slot="title">
          <xf-icon type="drive_logo" size="large"></xf-icon>
          <div class="title">
            Make everything in your Google Drive available when you're offline
          </div>
        </div>
        <div slot="body">
          <div class="description">
            This will automatically download all files in your My Drive,
            allowing you to access and edit your files without an internet
            connection.
          </div>
          <ul>
            <li>
              <xf-icon type="check"></xf-icon> Store all My Drive files in the cloud
              and on your computer
            </li>
            <li>
              <xf-icon type="check"></xf-icon> Access files from a folder on your
              computer
            </li>
            <li>
              <xf-icon type="check"></xf-icon> All files are automatically available
              offline
            </li>
          </ul>
          <div class="note">
            This will use about 12.2 GB leaving 96.8 GB available
          </div>
        </div>
        <div slot="button-container">
          <cr-button class="cancel-button" @click="${this.onCancel}"> ${
        this.copy_.cancel} </cr-button>
          <cr-button class="continue-button action-button" @click="${
        this.onContinue}"> ${this.copy_.continue} </cr-button>
        </div>
      </cr-dialog>
    `;
  }
}

function getCSS() {
  return css`
    cr-dialog [slot="body"] {
      color: var(--cros-text-color-secondary);
      display: flex;
      flex-direction: column;
      font-size: 14px;
      font-weight: 400;
      line-height: 20px;
    }

    cr-dialog [slot="title"] {
      display: flex;
      font-size: 18px;
      font-weight: 500;
      line-height: 24px;
    }

    cr-dialog [slot="title"] xf-icon {
      margin-right: 16px;
    }

    .description {
      margin-bottom: 24px;
    }

    ul {
      border-radius: 12px 12px 0 0;
      border: 1px solid var(--cros-separator-color);
      margin: 0;
      padding: 20px 18px;
    }

    .note {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 0 0 12px 12px;
      border-inline-end: 1px solid var(--cros-separator-color);
      border-block-end: 1px solid var(--cros-separator-color);
      border-inline-start: 1px solid var(--cros-separator-color);
      padding: 16px;
    }

    li {
      display: flex;
    }

    li + li {
      margin-top: 16px;
    }

    li > xf-icon {
      --xf-icon-color: var(--cros-icon-color-green);
      margin-right: 10px;
    }
  `;
}

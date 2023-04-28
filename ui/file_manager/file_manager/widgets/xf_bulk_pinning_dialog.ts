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
    continue: 'Continue',  // TODO: replace with final copy when available.
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
            Keep your files available when you’re offline
          </div>
        </div>
        <div slot="body">
          <div class="description">
            Everything in your My Drive will be synced automatically so you can
            access your files without an internet connection.
          </div>
          <ul>
            <li>
              <xf-icon type="my_files"></xf-icon>
              My Drive files are stored in the cloud and on this device
            </li>
            <li>
              <xf-icon type="offline"></xf-icon>
              Files will be automatically available offline
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
      color: var(--cros-sys-on_surface_variant);
      display: flex;
      flex-direction: column;
      font: var(--cros-body-1-font);
    }

    cr-dialog [slot="title"] {
      align-items: center;
      color: var(--cros-sys-on_surface);
      display: flex;
      font: var(--cros-display-7-font);
    }

    cr-dialog [slot="title"] xf-icon {
      --xf-icon-color: var(--cros-sys-primary);
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
      border-block-end: 1px solid var(--cros-separator-color);
      border-inline-end: 1px solid var(--cros-separator-color);
      border-inline-start: 1px solid var(--cros-separator-color);
      border-radius: 0 0 12px 12px;
      color: var(--cros-sys-on_surface);
      padding: 16px;
    }

    li {
      color: var(--cros-sys-on_surface);
      display: flex;
    }

    li + li {
      margin-top: 16px;
    }

    li > xf-icon {
      --xf-icon-color: var(--cros-sys-secondary);
      margin-right: 10px;
    }

    cr-button {
      --active-bg: transparent;
      --active-shadow: none;
      --active-shadow-action: none;
      --bg-action: var(--cros-sys-primary);
      --cr-button-height: 36px;
      --disabled-bg-action: var(--cros-sys-disabled_container);
      --disabled-bg: var(--cros-sys-disabled_container);
      --disabled-text-color: var(--cros-sys-disabled);
      --hover-bg-action: var(--cros-sys-primary);
      --hover-bg-color: var(--cros-sys-primary_container);
      --ink-color: var(--cros-sys-ripple_primary);
      --ripple-opacity-action: 1;
      --ripple-opacity: 1;
      --text-color-action: var(--cros-sys-on_primary);
      --text-color: var(--cros-sys-on_primary_container);
      border: none;
      border-radius: 18px;
      box-shadow: none;
      font: var(--cros-button-2-font);
      position: relative;
    }

    cr-button.cancel-button {
      background-color: var(--cros-sys-primary_container);
    }

    cr-button.cancel-button:hover::part(hoverBackground) {
      background-color: var(--cros-sys-hover_on_subtle);
      display: block;
    }

    cr-button.action-button:hover::part(hoverBackground) {
      background-color: var(--cros-sys-hover_on_prominent);
      display: block;
    }

    :host-context(.focus-outline-visible) cr-button:focus {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
    }
  `;
}

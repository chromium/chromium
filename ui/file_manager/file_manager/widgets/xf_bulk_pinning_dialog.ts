// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {str, strf, util} from '../common/js/util.js';

import {css, customElement, html, query, XfBase} from './xf_base.js';

/**
 * Different flavors of the XfBulkPinningDialog.
 */
const enum State {
  NORMAL = 'normal',
  ERROR = 'error',
  OFFLINE = 'offline',
}

/**
 * Dialog that shows the benefits of enabling bulk pinning along with storage
 * information if the feature can't be enabled.
 */
@customElement('xf-bulk-pinning-dialog')
export class XfBulkPinningDialog extends XfBase {
  @query('cr-dialog') private $dialog_!: CrDialogElement;
  @query('#point1') private $point1_!: HTMLElement;
  @query('#continue-button') private $button_!: CrButtonElement;
  @query('#normal-footer') private $normalFooter_!: HTMLElement;
  @query('#error-footer') private $errorFooter_!: HTMLElement;
  @query('#offline-footer') private $offlineFooter_!: HTMLElement;

  requiredBytes = 0;
  freeBytes = 0;

  set state(s: State) {
    // Show the footer matching the given state.
    this.$normalFooter_.style.display = s === State.NORMAL ? 'flex' : 'none';
    this.$errorFooter_.style.display = s === State.ERROR ? 'flex' : 'none';
    this.$offlineFooter_.style.display = s === State.OFFLINE ? 'flex' : 'none';
    // Enable or disable the 'Continue' button according to the given state.
    this.$button_.disabled = s !== State.NORMAL;
  }

  show() {
    this.$point1_.innerHTML = str('BULK_PINNING_POINT_1');
    this.$normalFooter_.innerText = strf(
        'BULK_PINNING_SPACE', util.bytesToString(this.requiredBytes),
        util.bytesToString(this.freeBytes - this.requiredBytes));
    this.$dialog_.showModal();
  }

  private onContinue() {
    this.$dialog_.close();
    chrome.fileManagerPrivate.setPreferences(
        {driveFsBulkPinningEnabled: true} as
        chrome.fileManagerPrivate.PreferencesChange);
  }

  private onCancel() {
    this.$dialog_.cancel();
  }

  /**
   * Called when the "View storage" link is clicked.
   */
  private onViewStorage(e: UIEvent) {
    e.preventDefault();
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  }

  override render() {
    return html`
      <cr-dialog>
        <div slot="title">
          <xf-icon type="drive_logo" size="large"></xf-icon>
          <div class="title">
            ${str('BULK_PINNING_TITLE')}
          </div>
        </div>
        <div slot="body">
          <div class="description">
            ${str('BULK_PINNING_EXPLANATION')}
          </div>
          <ul>
            <li>
              <xf-icon type="my_files"></xf-icon>
              <span id="point1"></span>
            </li>
            <li>
              <xf-icon type="offline"></xf-icon>
              ${str('BULK_PINNING_POINT_2')}
            </li>
          </ul>
          <div id="normal-footer"></div>
          <div id="error-footer">
            ${str('BULK_PINNING_NOT_ENOUGH_SPACE')}
            &ensp;
            <a href="_blank" @click="${this.onViewStorage}">
              ${str('BULK_PINNING_VIEW_STORAGE')}
            </a>
          </div>
          <div id="offline-footer">
            ${str('BULK_PINNING_OFFLINE')}
          </div>
        </div>
        <div slot="button-container">
          <cr-button class="cancel-button" @click="${this.onCancel}">
            ${str('CANCEL_LABEL')}
          </cr-button>
          <cr-button id="continue-button" class="continue-button action-button"
            @click="${this.onContinue}">
            ${str('BULK_PINNING_CONTINUE')}
          </cr-button>
        </div>
      </cr-dialog>
    `;
  }

  static override get styles() {
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
        border-bottom-style: none;
        margin: 0;
        padding: 20px 18px;
      }

      #normal-footer {
        background-color: var(--cros-sys-app_base_shaded);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_surface);
        padding: 16px;
      }

      #error-footer {
        background-color: var(--cros-sys-error_container);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_error_container);
        padding: 16px;
      }

      #offline-footer {
        background-color: var(--cros-sys-surface_variant);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_surface_variant);
        padding: 16px;
      }

      a {
        color: var(--cros-sys-on_error_container);
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
}

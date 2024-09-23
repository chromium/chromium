// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {calculateBulkPinRequiredSpace} from '../common/js/api.js';
import {RateLimiter} from '../common/js/async_util.js';
import {bytesToString, getCurrentLocaleOrDefault, str, strf} from '../common/js/translations.js';
import {visitURL} from '../common/js/util.js';
import type {State as AppState} from '../state/state.js';
import {getStore} from '../state/store.js';

import {css, customElement, html, query, XfBase} from './xf_base.js';

// Different flavors of the XfBulkPinningDialog.
const enum DialogState {
  // The dialog is not displayed.
  CLOSED,

  // Currently offline. Cannot compute space requirement for the time being.
  OFFLINE,

  // Currently not running due to battery saver mode active.
  BATTERY_SAVER,

  // Listing files and computing space requirements.
  LISTING,

  // An error occurred while computing the space requirements.
  ERROR,

  // There isn't enough space to activate the bulk-pinning feature.
  NOT_ENOUGH_SPACE,

  // Computed space requirements and ready to activate the bulk-pinning feature.
  READY,
}

export const BulkPinStage = chrome.fileManagerPrivate.BulkPinStage;

/**
 * Dialog that shows the benefits of enabling bulk pinning along with storage
 * information if the feature can't be enabled.
 */
@customElement('xf-bulk-pinning-dialog')
export class XfBulkPinningDialog extends XfBase {
  @query('cr-dialog') private $dialog_!: CrDialogElement;
  @query('#continue-button') private $button_!: CrButtonElement;
  @query('#offline-footer') private $offlineFooter_!: HTMLElement;
  @query('#battery-saver-footer') private $batterySaverFooter_!: HTMLElement;
  @query('#listing-footer') private $listingFooter_!: HTMLElement;
  @query('#error-footer') private $errorFooter_!: HTMLElement;
  @query('#not-enough-space-footer')
  private $notEnoughSpaceFooter_!: HTMLElement;
  @query('#ready-footer') private $readyFooter_!: HTMLElement;
  @query('#listing-files-text') private $listingFilesText_!: HTMLElement;

  private store_ = getStore();
  private stage_ = '';
  private requiredBytes_ = 0;
  private freeBytes_ = 0;
  private listedFiles_ = 0;

  private updateListedFilesDebounced_ =
      new RateLimiter(() => this.updateListedFiles_(), 5000);

  private updateListedFiles_() {
    if (this.listedFiles_ === 0) {
      this.$listingFilesText_.innerText = str('BULK_PINNING_LISTING');
    } else if (this.listedFiles_ === 1) {
      this.$listingFilesText_.innerText =
          str('BULK_PINNING_LISTING_WITH_SINGLE_ITEM');
    } else {
      this.$listingFilesText_.innerText = strf(
          'BULK_PINNING_LISTING_WITH_MULTIPLE_ITEMS',
          this.listedFiles_.toLocaleString(getCurrentLocaleOrDefault()));
    }
  }

  // Called when the app has changed state.
  onStateChanged(state: AppState) {
    // If bulk-pinning gets enabled while this dialog is open, just cancel this
    // dialog.
    if (state.preferences?.driveFsBulkPinningEnabled) {
      this.onCancel();
      return;
    }

    // We're only interested in the bulk-pinning part of the app state.
    const bpp = state.bulkPinning;
    if (!bpp) {
      return;
    }

    if (this.freeBytes_ !== bpp.freeSpaceBytes ||
        this.requiredBytes_ !== bpp.requiredSpaceBytes) {
      this.freeBytes_ = bpp.freeSpaceBytes;
      this.requiredBytes_ = bpp.requiredSpaceBytes;
      this.$readyFooter_.innerText = strf(
          'BULK_PINNING_SPACE', bytesToString(this.requiredBytes_),
          bytesToString(this.freeBytes_));
    }

    if (bpp.stage === BulkPinStage.LISTING_FILES && bpp.listedFiles > 0 &&
        bpp.listedFiles !== this.listedFiles_) {
      this.listedFiles_ = bpp.listedFiles;
      this.updateListedFilesDebounced_.run();
    }

    if (bpp.stage === this.stage_) {
      return;
    }

    this.stage_ = bpp.stage;
    switch (bpp.stage) {
      case BulkPinStage.PAUSED_OFFLINE:
        this.state = DialogState.OFFLINE;
        break;

      case BulkPinStage.PAUSED_BATTERY_SAVER:
        this.state = DialogState.BATTERY_SAVER;
        break;

      case BulkPinStage.GETTING_FREE_SPACE:
      case BulkPinStage.LISTING_FILES:
        this.state = DialogState.LISTING;
        break;

      case BulkPinStage.SUCCESS:
        this.state = DialogState.READY;
        break;

      case BulkPinStage.SYNCING:
        this.$dialog_.close();
        break;

      case BulkPinStage.NOT_ENOUGH_SPACE:
        this.state = DialogState.NOT_ENOUGH_SPACE;
        break;

      default:
        console.warn(
            `Cannot calculate bulk-pinning space requirements: ${this.stage_}`);
        this.state = DialogState.ERROR;
        break;
    }
  }

  // Shows the footer matching the given state.
  // Enables or disables the 'Continue' button according to the given state.
  set state(s: DialogState) {
    this.$offlineFooter_.style.display =
        s === DialogState.OFFLINE ? 'initial' : 'none';
    this.$batterySaverFooter_.style.display =
        s === DialogState.BATTERY_SAVER ? 'initial' : 'none';
    this.$listingFooter_.style.display =
        s === DialogState.LISTING ? 'flex' : 'none';
    this.$errorFooter_.style.display =
        s === DialogState.ERROR ? 'initial' : 'none';
    this.$notEnoughSpaceFooter_.style.display =
        s === DialogState.NOT_ENOUGH_SPACE ? 'initial' : 'none';
    this.$readyFooter_.style.display =
        s === DialogState.READY ? 'initial' : 'none';

    this.$button_.disabled = s !== DialogState.READY;
  }

  // Indicates if this dialog is currently open.
  get is_open(): boolean {
    return this.$dialog_.open;
  }

  // Shows the dialog and starts calculating the required space for
  // bulk-pinning.
  async show() {
    this.stage_ = BulkPinStage.LISTING_FILES;
    this.state = DialogState.LISTING;
    this.$dialog_.showModal();
    this.store_.subscribe(this);
    try {
      await calculateBulkPinRequiredSpace();
    } catch (e) {
      console.error('Cannot calculate required space for bulk-pinning:', e);
      this.state = DialogState.ERROR;
    }
  }

  private onClose(_: Event) {
    this.state = DialogState.CLOSED;
    this.listedFiles_ = 0;
    this.updateListedFilesDebounced_.runImmediately();
    this.store_.unsubscribe(this);
  }

  // Called when the "Continue" button is clicked.
  private onContinue() {
    this.$dialog_.close();
    chrome.fileManagerPrivate.setPreferences(
        {driveFsBulkPinningEnabled: true} as
        chrome.fileManagerPrivate.PreferencesChange);
  }

  // Called when the "Cancel" button is clicked.
  private onCancel() {
    this.$dialog_.cancel();
  }

  // Called when the "Learn more" link is clicked.
  private onLearnMore(e: UIEvent) {
    e.preventDefault();
    visitURL('https://support.google.com/chromebook?p=my_drive_cbx');
  }

  // Called when the "View storage" link is clicked.
  private onViewStorage(e: UIEvent) {
    e.preventDefault();
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  }

  override render() {
    return html`
      <cr-dialog @close="${this.onClose}">
        <div slot="title">
          <xf-icon type="drive_bulk_pinning" size="medium"></xf-icon>
          <div class="title" style="flex: 1 0 0">
            ${str('BULK_PINNING_TITLE')}
          </div>
        </div>
        <div slot="body">
          <div class="description">
            ${str('BULK_PINNING_EXPLANATION')}
            <a id="learn-more-link" href="_blank" @click="${this.onLearnMore}">
              ${str('LEARN_MORE_LABEL')}
            </a>
          </div>
          <ul>
            <li>
              <xf-icon type="my_files"></xf-icon>
              ${str('BULK_PINNING_POINT_1')}
            </li>
          </ul>
          <div id="offline-footer" class="offline-footer">
            ${str('BULK_PINNING_OFFLINE')}
          </div>
          <div id="battery-saver-footer" class="battery-saver-footer">
            ${str('BULK_PINNING_BATTERY_SAVER')}
          </div>
          <div id="listing-footer" class="normal-footer">
            <files-spinner></files-spinner>
            <span id="listing-files-text">
              ${str('BULK_PINNING_LISTING')}
            </span>
          </div>
          <div id="error-footer" class="error-footer">
            ${str('BULK_PINNING_ERROR')}
          </div>
          <div id="not-enough-space-footer" class="error-footer">
            ${str('BULK_PINNING_NOT_ENOUGH_SPACE')}
            <a id="view-storage-link" href="_blank"
              @click="${this.onViewStorage}">
              ${str('BULK_PINNING_VIEW_STORAGE')}
            </a>
          </div>
          <div id="ready-footer" class="normal-footer"></div>
        </div>
        <div slot="button-container">
          <cr-button id="cancel-button" class="cancel-button"
            @click="${this.onCancel}">
            ${str('CANCEL_LABEL')}
          </cr-button>
          <cr-button id="continue-button" class="continue-button action-button"
            @click="${this.onContinue}">
            ${str('BULK_PINNING_TURN_ON')}
          </cr-button>
        </div>
      </cr-dialog>
    `;
  }

  static override get styles() {
    return css`
      cr-dialog {
        --cr-dialog-background-color: var(--cros-sys-dialog_container);
        --cr-dialog-body-padding-horizontal: 0;
        --cr-dialog-button-container-padding-bottom: 0;
        --cr-dialog-button-container-padding-horizontal: 0;
        --cr-dialog-title-slot-padding-bottom: 16px;
        --cr-dialog-title-slot-padding-end: 0;
        --cr-dialog-title-slot-padding-start: 0;
        --cr-dialog-title-slot-padding-top: 0;
        --cr-primary-text-color: var(--cros-sys-on_surface);
        --cr-secondary-text-color: var(--cros-sys-on_surface_variant);
      }

      cr-dialog::part(dialog) {
        border-radius: 20px;
      }

      cr-dialog::part(dialog)::backdrop {
        background-color: var(--cros-sys-scrim);
      }

      cr-dialog::part(wrapper) {
        padding: 32px;
        padding-bottom: 28px;
      }

      cr-dialog [slot="body"] {
        display: flex;
        flex-direction: column;
        font: var(--cros-body-1-font);
      }

      cr-dialog [slot="button-container"] {
        padding-top: 32px;
      }

      cr-dialog [slot="title"] {
        align-items: center;
        display: flex;
        font: var(--cros-display-7-font);
      }

      cr-dialog [slot="title"] xf-icon {
        --xf-icon-color: var(--cros-sys-primary);
        margin-inline-end: 16px;
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

      .normal-footer {
        align-items: center;
        background-color: var(--cros-sys-app_base_shaded);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_surface);
        padding: 16px;
      }

      .error-footer {
        background-color: var(--cros-sys-error_container);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_error_container);
        padding: 16px;
      }

      .offline-footer, .battery-saver-footer {
        background-color: var(--cros-sys-surface_variant);
        border: 1px solid var(--cros-separator-color);
        border-radius: 0 0 12px 12px;
        border-top-style: none;
        color: var(--cros-sys-on_surface_variant);
        padding: 16px;
      }

      a {
        color: var(--cros-sys-primary);
      }

      .error-footer > a {
        color: inherit;
      }

      files-spinner {
        transform: scale(0.666);
        margin: 0;
        margin-inline-end: 10px;
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
        margin-inline-end: 10px;
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

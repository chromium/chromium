// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, strf} from '../common/js/translations.js';
import type {FilesTooltip} from '../foreground/elements/files_tooltip.js';

import {css, customElement, html, property, XfBase} from './xf_base.js';

const SyncStatus = chrome.fileManagerPrivate.SyncStatus;

/**
 * Sync status element used in both table and grid views that indicates sync
 * progress and file pinning.
 */
@customElement('xf-inline-status')
export class XfInlineStatus extends XfBase {
  @property({type: Boolean, reflect: true, attribute: 'cant-pin'})
  cantPin = false;
  @property({type: Boolean, reflect: true, attribute: 'available-offline'})
  availableOffline = false;
  @property({type: Number, reflect: true}) progress = 0;
  @property({type: SyncStatus, reflect: true, attribute: 'sync-status'})
  syncStatus = SyncStatus.NOT_FOUND;

  override connectedCallback() {
    super.connectedCallback();
    document.querySelector<FilesTooltip>('files-tooltip')!.addTarget(this);
  }

  static override get styles() {
    return getCSS();
  }

  override render() {
    const {progress, syncStatus, availableOffline, cantPin} = this;

    if (syncStatus !== SyncStatus.NOT_FOUND) {
      // Syncing, hence displaying "queued" or "in progress".
      this.ariaLabel = progress === 0 ?
          str('QUEUED_LABEL') :
          strf('IN_PROGRESS_PERCENTAGE_LABEL', (progress * 100).toFixed(0));
      return html`<xf-pie-progress progress=${progress} />`;
    }

    if (cantPin) {
      this.ariaLabel = str('DRIVE_ITEM_UNAVAILABLE_OFFLINE');
      return this.renderIcon_('cant-pin');
    }

    if (availableOffline) {
      this.ariaLabel = str('OFFLINE_COLUMN_LABEL');
      return this.renderIcon_('offline');
    }

    this.ariaLabel = '';
    return html``;
  }

  private renderIcon_(type: string) {
    return html`<xf-icon size="extra_small" type="${type}" />`;
  }
}

function getCSS() {
  return css`
    xf-pie-progress, xf-icon {
      display: flex;
      height: 16px;
      width: 16px;
    }

    xf-icon {
      --xf-icon-color: currentColor;
    }
  `;
}

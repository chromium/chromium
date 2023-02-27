// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, customElement, html, property, query, XfBase} from './xf_base.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

@customElement('xf-cloud-panel')
export class XfCloudPanel extends XfBase {
  @property({type: Boolean, attribute: false}) shouldRenderProgress = false;

  @query('cr-action-menu') private $panel_?: CrActionMenuElement;

  static override get styles() {
    return getCSS();
  }

  override connectedCallback() {
    super.connectedCallback();

    // Work in progress. For testing.
    setInterval(
        () => (this.shouldRenderProgress = !this.shouldRenderProgress), 3000);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  showAt(el: HTMLElement) {
    this.$panel_!.showAt(el, {top: el.offsetTop + el.offsetHeight + 8});
  }

  override render() {
    return html`<cr-action-menu>
      <div class="body">
        ${
        this.shouldRenderProgress ? html`
              <div class="progress">Syncing 28 files</div>
              <div class="progress-track">
                <div class="progress-bar"></div>
              </div>
              <div class="progress-description">3 minutes remaining</div>
            ` :
                                    html`
              <xf-icon class="status-icon" type="error" size="large"></xf-icon>
              <div class="status-description">
                You’ve used all your 15 GB of storage. To resume syncing, free
                up space or get more storage.
              </div>
            `}

        <div class="divider"></div>
        <div class="menu">
          <div class="action">Google Drive settings</div>
        </div>
      </div>
    </cr-action-menu>`;
  }
}

function getCSS() {
  return css`
    :host {
      position: absolute;
      right: 0px;
      top: 50px;
      z-index: 600;
    }

    .body {
      margin: -8px 0;
      width: 320px;
      display: flex;
      flex-direction: column;
    }

    .status-icon {
      --xf-icon-color: transparent;
      padding: 27px 0px 20px;
      align-self: center;
    }

    .status-description {
      color: var(--cros-text-color-secondary);
      font-size: 13px;
      line-height: 20px;
      padding: 0px 16px 20px;
      text-align: center;
    }

    .progress {
      color: var(--cros-text-color-primary);
      font-size: 13px;
      font-weight: 500;
      line-height: 20px;
      padding: 20px 16px 8px;
    }

    .progress-description {
      color: var(--cros-text-color-secondary);
      padding: 0px 16px 20px;
    }

    .progress-track {
      background: #dbe1ff;
      border-radius: 2px;
      height: 4px;
      margin: 0px 16px 8px;
      overflow: hidden;
    }

    .progress-bar {
      background: #3f5aa9;
      border-radius: 2px;
      height: 100%;
      width: 50%;
    }

    .divider {
      background: rgba(27, 27, 31, 0.14);
      height: 1px;
      width: 100%;
    }

    .action {
      font-size: 13px;
      font-weight: 500;
      line-height: 20px;
      padding: 20px 16px;
    }

    .action:hover {
      background: var(--cros-menu-item-background-hover);
    }
  `;
}

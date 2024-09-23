// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrDrawerElement} from './cr_drawer.js';

export function getHtml(this: CrDrawerElement) {
  return html`
<dialog id="dialog" @cancel="${this.onDialogCancel_}"
    @click="${this.onDialogClick_}" @close="${this.onDialogClose_}">
  <div id="container" @click="${this.onContainerClick_}">
    <div class="drawer-header">
      <slot name="header-icon">
        <picture>
          <source media="(prefers-color-scheme: dark)"
              srcset="//resources/images/chrome_logo_dark.svg">
          <img id="product-logo"
              srcset="chrome://theme/current-channel-logo@1x 1x,
                      chrome://theme/current-channel-logo@2x 2x"
              role="presentation">
        </picture>
      </slot>
      <div id="heading" tabindex="-1">${this.heading}</div>
    </div>
    <slot name="body"></slot>
  </div>
</dialog>`;
}

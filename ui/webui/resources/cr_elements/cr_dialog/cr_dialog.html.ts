// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrDialogElement} from './cr_dialog.js';

export function getHtml(this: CrDialogElement) {
  // clang-format off
  return html`
<dialog id="dialog" @close="${this.onNativeDialogClose_}"
    @cancel="${this.onNativeDialogCancel_}" part="dialog"
    aria-labelledby="title"
    aria-description="${this.ariaDescriptionText || nothing}">
<!-- This wrapper is necessary, such that the "pulse" animation is not
    erroneously played when the user clicks on the outer-most scrollbar. -->
  <div id="content-wrapper" part="wrapper">
    <div class="top-container">
      <h2 id="title" class="title-container" tabindex="-1">
        <slot name="title"></slot>
      </h2>
      ${this.showCloseButton ? html`
        <cr-icon-button id="close" class="icon-clear"
            aria-label="${this.closeText || nothing}"
            @click="${this.cancel}" @keypress="${this.onCloseKeypress_}">
        </cr-icon-button>
       ` : ''}
    </div>
    <slot name="header"></slot>
    <div class="body-container" id="container" show-bottom-shadow
        part="body-container">
      <slot name="body"></slot>
    </div>
    <slot name="button-container"></slot>
    <slot name="footer"></slot>
  </div>
</dialog>`;
  // clang-format on
}

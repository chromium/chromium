// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrActionMenuElement} from './cr_action_menu.js';

export function getHtml(this: CrActionMenuElement) {
  return html`
<dialog id="dialog" part="dialog" @close="${this.onNativeDialogClose_}"
    role="application"
    aria-roledescription="${this.roleDescription || nothing}">
  <div id="wrapper" class="item-wrapper" role="menu" tabindex="-1"
      aria-label="${this.accessibilityLabel || nothing}">
    <slot id="contentNode" @slotchange="${this.onSlotchange_}"></slot>
  </div>
</dialog>`;
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToastManagerElement} from './cr_toast_manager.js';

export function getHtml(this: CrToastManagerElement) {
  return html`
<cr-toast id="toast" .duration="${this.duration}">
  <div id="content" class="elided-text"></div>
  <slot id="slotted"></slot>
</cr-toast>`;
}

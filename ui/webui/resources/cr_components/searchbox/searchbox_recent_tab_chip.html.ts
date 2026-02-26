// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`
${this.computeShowRecentTabChip_() ? html`
  <div id="recentTabChipContainer">
    <composebox-recent-tab-chip class="upload-button contextual-chip"
        .recentTab="${this.recentTabForChip_}"
        @add-tab-context="${this.addTabContext_}">
    </composebox-recent-tab-chip>
  </div>
` : nothing}
`;
  // clang-format on
}

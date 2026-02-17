// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`
<cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
    class="${!this.ntpRealboxNextEnabled ? 'dropdownContainer' : nothing}"
    exportparts="dropdown-content"
    role="listbox" .result="${this.result_}"
    selected-match-index="${this.selectedMatchIndex_}"
    @selected-match-index-changed="${this.onSelectedMatchIndexChanged_}"
    ?can-show-secondary-side="${this.canShowSecondarySide}"
    ?had-secondary-side="${this.hadSecondarySide}"
    @had-secondary-side-changed="${this.onHadSecondarySideChanged_}"
    ?has-secondary-side="${this.hasSecondarySide}"
    @has-secondary-side-changed="${this.onHasSecondarySideChanged_}"
    @match-focusin="${this.onMatchFocusin_}"
    @match-click="${this.onMatchClick_}"
    ?hidden="${!this.dropdownIsVisible}"
    ?show-thumbnail="${this.showThumbnail}">
</cr-searchbox-dropdown>`;
}

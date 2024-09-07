// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchQueryElement} from './search_query.js';

export function getHtml(this: SearchQueryElement) {
  return html`
<a id="searchQueryLink" class="pill pill-icon-start"
    href="${this.searchQuery?.url.url || nothing}"
    @click="${this.onClick_}" @auxclick="${this.onAuxClick_}"
    @keydown="${this.onKeydown_}">
  <div id="hover-layer"></div>
  <span class="icon cr-icon"></span>
  <span class="truncate">${this.searchQuery?.query || ''}</span>
</a>`;
}

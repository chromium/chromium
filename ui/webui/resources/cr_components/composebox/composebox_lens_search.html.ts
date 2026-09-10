// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxLensSearchElement} from './composebox_lens_search.js';

export function getHtml(this: ComposeboxLensSearchElement) {
  // clang-format off
  return html`
    ${this.isIcon ? html`
      <cr-icon-button id="lensIcon"
          part="lens-icon"
          iron-icon="composebox:google-lens-2"
          title="${this.i18n('lensSearchHint')}"
          aria-label="${this.i18n('lensSearchHint')}"
          @click="${this.onLensSearchClick_}">
      </cr-icon-button>
    ` : html`
      <cr-button id="lensSearchPill"
          part="lens-search-pill"
          title="${this.i18n('lensSearchHint')}"
          aria-label="${this.i18n('lensSearchHint')}"
          @click="${this.onLensSearchClick_}">
        <div id="content">
          <div>${this.i18n('lensSearchHint')}</div>
        </div>
      </cr-button>
    `}
  `;
  // clang-format on
}

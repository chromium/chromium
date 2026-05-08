// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxLensSearchElement} from './composebox_lens_search.js';

export function getHtml(this: ComposeboxLensSearchElement) {
  // clang-format off
  return html`
  <cr-button id="lensSearch"
      aria-label="${this.i18n('lensSearchHint')}" title="${this.i18n('lensSearchHint')}"
      @click="${this.onLensSearchClick_}">
    <div id="content">
      <div>${this.i18n('lensSearchHint')}</div>
    </div>
  </cr-button>`;
  // clang-format on
}

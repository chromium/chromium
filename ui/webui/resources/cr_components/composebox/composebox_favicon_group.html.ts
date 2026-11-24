// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFaviconGroupElement} from './composebox_favicon_group.js';

export function getHtml(this: ComposeboxFaviconGroupElement) {
  return html`
${this.visibleTabs_.map(item => html`
  <div class="favicon-item ${this.isTabLoading_(item) ? 'loading' : ''}"
      style="--favicon-image: ${this.getFaviconUrl_(item)}">
  </div>
`)}
${this.remainingCount_ > 0 ? html`
  <div id="more-items">
    +${this.remainingCount_}
  </div>
` : ''}`;
}

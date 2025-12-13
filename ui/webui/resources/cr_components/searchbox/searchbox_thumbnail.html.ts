// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxThumbnailElement} from './searchbox_thumbnail.js';

export function getHtml(this: SearchboxThumbnailElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" aria-hidden="true">
  <img id="image" src="${this.thumbnailUrl_}">
  <div class="overlay">
    ${this.isDeletable_ ? html`
      <cr-icon-button id="remove" class="action-icon icon-clear"
          @click="${this.onRemoveButtonClick_}">
      </cr-icon-button>
    ` : ''}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}

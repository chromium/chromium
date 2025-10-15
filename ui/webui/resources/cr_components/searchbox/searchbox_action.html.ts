// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxActionElement} from './searchbox_action.js';

export function getHtml(this: SearchboxActionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="overlay"></div>
<div class="contents" title="${this.suggestionContents}">
  <div id="action-icon" style="${this.iconStyle_}"></div>
  <div id="text" .innerHTML="${this.hintHtml_}"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}

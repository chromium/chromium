// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxMatchElement} from './composebox_match.js';

export function getHtml(this: ComposeboxMatchElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="container" aria-hidden="true" part="match-container">
  <div id="focusIndicator" part="match-focus-indicator"></div>
  <div id="iconContainer" part="match-icon-container">
    <div id="icon" part="match-icon"
        style="-webkit-mask-image: url(${this.iconPath_()});">
    </div>
  </div>
  <div id="textContainer" part="match-text-container"></div>
  <cr-icon-button id="remove" class="action-icon icon-clear"
    part="match-remove-button"
    aria-label="${this.computeRemoveButtonAriaLabel_()}"
    @click="${this.onRemoveButtonClick_}"
    @mousedown="${this.onRemoveButtonMouseDown_}"
    title="${this.removeButtonTitle_}"
    ?hidden="${!this.match.supportsDeletion}"
    tabindex="2">
  </cr-icon-button>
</div>
  <!--_html_template_end_-->`;
  // clang-format on
}

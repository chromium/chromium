// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxDropdownElement} from './searchbox_dropdown.js';

export function getHtml(this: SearchboxDropdownElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content" part="dropdown-content">
  ${this.sideTypes_().map(sideType => html`
    <div class="${this.sideTypeClass_(sideType)}">
      ${this.groupIdsForSideType_(sideType).map(groupId => html`
        ${this.hasHeaderForGroup_(groupId) ? html`
          <!-- Header cannot be tabbed into but gets focus when clicked. This
              stops the dropdown from losing focus and closing as a result. -->
          <h3 class="header" data-id="${groupId}" tabindex="-1"
              @mousedown="${this.onHeaderMousedown_}" aria-hidden="true">
            <span class="text">${this.headerForGroup_(groupId)}</span>
          </h3>
        ` : ''}
        <div class="matches ${this.renderTypeClassForGroup_(groupId)}">
          ${this.matchesForGroup_(groupId).map(match => html`
            <cr-searchbox-match tabindex="0" role="option"
                .match="${match}" match-index="${this.matchIndex_(match)}"
                side-type="${sideType}"
                ?selected="${this.isSelected_(match)}"
                ?show-thumbnail="${this.showThumbnail}">
            </cr-searchbox-match>
          `)}
        </div>
      `)}
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import type {SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import type {SearchboxDropdownElement} from './searchbox_dropdown.js';

export function getMatchesForGroupHtml(
    this: SearchboxDropdownElement, sideType: SideType, groupId: number) {
  const hasHeader = this.hasHeaderForGroup_(groupId);
  // clang-format off
  const matchesHtml = html`
    <div class="matches ${this.renderTypeClassForGroup_(groupId)}">
    ${this.matchesForGroup_(groupId).map(match => html`
      <cr-searchbox-match tabindex="0" role="option"
          aria-describedby="${hasHeader ? `hg_${groupId}` : nothing}"
          .match="${match}" match-index="${this.matchIndex_(match)}"
          side-type="${sideType}"
          ?selected="${this.isSelected_(match)}"
          ?show-thumbnail="${this.showThumbnail}">
      </cr-searchbox-match>
    `)}
    </div>
  `;
  return hasHeader ? html`
    <!-- Header cannot be tabbed into but gets focus when clicked. This
        stops the dropdown from losing focus and closing as a result. -->
    <h3 class="header" data-id="${groupId}" tabindex="-1"
        id="hg_${groupId}"
        @mousedown="${this.onHeaderMousedown_}">
        <span class="text">${this.headerForGroup_(groupId)}</span>
    </h3>
    ${matchesHtml}` : matchesHtml;
  // clang-format on
}

export function getHtml(this: SearchboxDropdownElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="content" part="dropdown-content">
${this.sideTypes_().map(sideType => html`
  <div class="${this.sideTypeClass_(sideType)}">
    ${this.groupIdsForSideType_(sideType)
        .map(groupId => getMatchesForGroupHtml.bind(this)(sideType, groupId))}
  </div>
`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}

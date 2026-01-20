// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualActionMenuElement} from './contextual_action_menu.js';

export function getHtml(this: ContextualActionMenuElement) {
  return html`<!--_html_template_start_-->
  <cr-action-menu id="menu" role-description="${this.i18n('menu')}"
      @close="${this.onMenuClose_}">
    ${this.tabSuggestions?.length > 0 ? html`
      <h4 id="tabHeader">${this.i18n('addTab')}</h4>
      ${this.tabSuggestions.map((tab, index) => html`
        <div class="suggestion-container">
          <button class="dropdown-item"
              title="${tab.title}" data-index="${index}"
              aria-label="${this.i18n('addTab')}, ${tab.title}"
              ?disabled="${this.isTabDisabled_(tab)}"
              @pointerenter="${this.onTabPointerenter_}"
              @click="${this.onTabClick_}">
            <cr-composebox-tab-favicon .url="${tab.url.url}">
            </cr-composebox-tab-favicon>
            <span class="tab-title">${tab.title}</span>
            ${this.enableMultiTabSelection_ ? html`
              ${this.disabledTabIds.has(tab.tabId) ? html`
                <cr-icon class="multi-tab-icon"
                    icon="cr:check" id="multi-tab-check"></cr-icon>
              ` : html`
                <cr-icon class="multi-tab-icon"
                    icon="cr:add" id="multi-tab-add"></cr-icon>
              `}
            ` : ''}
          </button>
          ${this.shouldShowTabPreview_() ? html`
            <img class="tab-preview" .src="${this.tabPreviewUrl_}">
          ` : ''}
        </div>
      `)}
      <hr/>
    `: ''}
    <button id="imageUpload" class="dropdown-item"
        @click="${this.openImageUpload_}"
         ?disabled="${this.imageUploadDisabled_}">
      <cr-icon icon="composebox:imageUpload"></cr-icon>
      ${this.i18n('addImage')}
    </button>
    ${this.pdfUploadEnabled_ ? html`<button id="fileUpload" class="dropdown-item"
        @click="${this.openFileUpload_}"
        ?disabled="${this.fileUploadDisabled_}">
      <cr-icon icon="composebox:fileUpload"></cr-icon>
      ${this.i18n('uploadFile')}
    </button>`: ''}
  </cr-action-menu>
<!--_html_template_end_-->`;
}

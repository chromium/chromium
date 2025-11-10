// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextMenuEntrypointElement} from './context_menu_entrypoint.js';

export function getHtml(this: ContextMenuEntrypointElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${this.showContextMenuDescription ? html`
    <cr-button id="entrypoint"
        @click="${this.onEntrypointClick_}"
        ?disabled="${this.inputsDisabled}"
        title="${this.i18n('addContextTitle')}">
      <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
      <span id="description">${this.i18n('addContext')}</span>
    </cr-button>
    `: html`
      <cr-icon-button id="entrypoint"
          part="context-menu-entrypoint-icon"
          iron-icon="cr:add"
          @click="${this.onEntrypointClick_}"
          ?disabled="${this.inputsDisabled}"
          title="${this.i18n('addContextTitle')}">
      </cr-icon-button>
    `}

  <cr-action-menu id="menu" role-description="${this.i18n('menu')}">
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
    <button id="fileUpload" class="dropdown-item"
        @click="${this.openFileUpload_}"
        ?disabled="${this.fileUploadDisabled_}">
      <cr-icon icon="composebox:fileUpload"></cr-icon>
      ${this.i18n('uploadFile')}
    </button>
    ${this.showDeepSearch_ || this.showCreateImage_ ? html`<hr/>` : ''}
    ${this.showDeepSearch_ ?
    html`<button id="deepSearch" class="dropdown-item"
        @click="${this.onDeepSearchClick_}"
        ?disabled="${this.deepSearchDisabled_}">
      <cr-icon icon="composebox:deepSearch"></cr-icon>
      ${this.i18n('deepSearch')}
    </button>` : ''}
    ${this.showCreateImage_ ?
    html`<button id="createImage" class="dropdown-item"
        @click="${this.onCreateImageClick_}"
        ?disabled="${this.createImageDisabled_}">
      <cr-icon icon="composebox:nanoBanana"></cr-icon>
      ${this.i18n('createImages')}
    </button>` : ''}
  </cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format off
}

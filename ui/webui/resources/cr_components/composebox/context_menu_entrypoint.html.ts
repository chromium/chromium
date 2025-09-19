// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextMenuEntrypointElement} from './context_menu_entrypoint.js';

export function getHtml(this: ContextMenuEntrypointElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <cr-button id="entrypoint"
      @click="${this.onEntrypointClick_}"
      ?disabled="${this.inputsDisabled}"
      title="${this.i18n('addContextTitle')}">
    <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
    <span id="description">${this.i18n('addContext')}</span>
  </cr-button>

  <cr-action-menu id="menu" role-description="${this.i18n('menu')}">
    ${this.tabSuggestions.length > 0 ? html`
      <h4 id="tabHeader">${this.i18n('addTab')}</h4>
      ${this.tabSuggestions.map((tab, index) => html`
        <button class="dropdown-item" title="${tab.title}"
            data-index="${index}" @click="${this.addTabContext}">
          <composebox-tab-favicon .url="${tab.url.url}">
          </composebox-tab-favicon>
          <span class="tab-title">${tab.title}</span>
        </button>
      `)}
      <hr/>
    `: ''}
    <button id="imageUpload" class="dropdown-item"
        @click="${this.openImageUpload}">
      <cr-icon icon="composebox:imageUpload"></cr-icon>
      ${this.i18n('addImage')}
    </button>
    <button id="fileUpload" class="dropdown-item"
        @click="${this.openFileUpload}">
      <cr-icon icon="composebox:fileUpload"></cr-icon>
      ${this.i18n('uploadFile')}
    </button>
  </cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format off
}

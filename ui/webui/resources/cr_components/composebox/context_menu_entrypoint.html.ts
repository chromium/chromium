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
      title="$i18n{addContextTitle}">
    <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
    <span id="description">$i18n{addContext}</span>
  </cr-button>

  <cr-action-menu id="menu" role-description="$i18n{menu}">
    ${this.tabSuggestions_.length > 0 ? html`
      <h4 id="tabHeader">$i18n{addTab}</h4>
      ${this.tabSuggestions_.map(tab => html`
        <cr-button class="dropdown-item" title="${tab.title}">
          <composebox-tab-favicon .url="${tab.url.url}" slot="prefix-icon">
          </composebox-tab-favicon>
          <span class="tab-title">${tab.title}</span>
        </cr-button>
      `)}
      <hr/>
    `: ''}
    <cr-button id="imageUpload" class="dropdown-item"
        @click="${this.openImageUpload}">
      <cr-icon icon="composebox:imageUpload" slot="prefix-icon"></cr-icon>
      $i18n{addImage}
    </cr-button>
    <cr-button id="fileUpload" class="dropdown-item"
        @click="${this.openFileUpload}">
      <cr-icon icon="composebox:fileUpload" slot="prefix-icon"></cr-icon>
      $i18n{uploadFile}
    </cr-button>
  </cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format off
}

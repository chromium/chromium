// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';

export function getHtml(this: ContextualEntrypointAndMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.showModelPicker ?  html`
    ${this.hasAllowedInputs_() ? html`
      <cr-composebox-contextual-entrypoint-button id="entrypointButton"
          exportparts="context-menu-entrypoint-icon, entrypoint-button"
          @context-menu-entrypoint-click="${this.showMenuAtEntrypoint_}"
          ?upload-button-disabled="${this.uploadButtonDisabled}"
          ?show-context-menu-description="${this.showContextMenuDescription}"
          glif-animation-state="${this.glifAnimationState}">
      </cr-composebox-contextual-entrypoint-button>
      <cr-composebox-contextual-action-menu id="menu"
          .fileNum="${this.fileNum}"
          .disabledTabIds="${this.disabledTabIds}"
          .tabSuggestions="${this.tabSuggestions}"
          .inputState="${this.inputState}"
          @close="${this.onMenuClose_}">
      </cr-composebox-contextual-action-menu>
    ` : nothing}
  ` : html`
      <!-- TODO(crbug.com/476467436): Remove the context-menu-entrypoint option
      once obsolete. -->
    <cr-composebox-context-menu-entrypoint id="entrypointMenu"
        exportparts="context-menu-entrypoint-icon"
        .tabSuggestions="${this.tabSuggestions}"
        .inCreateImageMode="${this.inCreateImageMode}"
        .hasImageFiles="${this.hasImageFiles}"
        .disabledTabIds="${this.disabledTabIds}"
        .fileNum="${this.fileNum}"
        .searchboxLayoutMode="${this.searchboxLayoutMode}"
        ?upload-button-disabled="${this.uploadButtonDisabled}"
        ?show-context-menu-description="${this.showContextMenuDescription}"
        glif-animation-state="${this.glifAnimationState}">
    </cr-composebox-context-menu-entrypoint>`}
  <!--_html_template_end_-->`;
  // clang-format on
}

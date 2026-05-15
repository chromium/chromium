// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {hasAllowedInputs} from './common.js';
import type {ContextualEntrypointAndMenuElement} from './contextual_entrypoint_and_menu.js';

export function getHtml(this: ContextualEntrypointAndMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${hasAllowedInputs(this.inputState, this.usePecApi) ? html`
      <cr-composebox-contextual-entrypoint-button id="entrypointButton"
          exportparts="context-menu-entrypoint-icon, entrypoint-button"
          .inputState="${this.inputState}"
          @context-menu-entrypoint-click="${this.onContextMenuEntrypointClick_}"
          ?upload-button-disabled="${this.uploadButtonDisabled}"
          ?show-context-menu-description="${this.showContextMenuDescription}"
          glif-animation-state="${this.glifAnimationState}">
      </cr-composebox-contextual-entrypoint-button>
    ` : ''}
    <cr-composebox-contextual-action-menu id="menu"
        .fileNum="${this.fileNum}"
        .disabledTabIds="${this.disabledTabIds}"
        .tabSuggestions="${this.tabSuggestions}"
        .inputState="${this.inputState}"
        .smartTabSharingActive="${this.smartTabSharingActive}"
        .disableAutoReposition="${this.disableAutoReposition}"
        @close="${this.onMenuClose_}">
    </cr-composebox-contextual-action-menu>
  <!--_html_template_end_-->`;
  // clang-format on
}

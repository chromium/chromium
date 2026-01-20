// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {GlifAnimationState} from './context_menu_entrypoint.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';

export function getHtml(this: ContextualEntrypointButtonElement) {
  // clang-format off
  const entrypointButton = !this.hideEntrypointButton ? html`
    ${this.showContextMenuDescription ? html`
    <cr-button id="entrypoint"
        class="ai-mode-button"
        @click="${this.onEntrypointClick_}"
        ?disabled="${this.inputsDisabled}"
        title="${this.i18n('addContextTitle')}"
        noink>
      <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
      <span id="description"
        @animationend="${(e: AnimationEvent) => {
          this.onAnimationEnd_(e, 'slide-in');
        }}">
          ${this.i18n('addContext')}
      </span>
    </cr-button>` : html`
    <cr-icon-button id="entrypoint"
        class="ai-mode-button"
        part="context-menu-entrypoint-icon"
        iron-icon="cr:add"
        @click="${this.onEntrypointClick_}"
        ?disabled="${this.inputsDisabled}"
        title="${this.i18n('addContextTitle')}"
        noink>
    </cr-icon-button>`}` : '';
  return html`<!--_html_template_start_-->
    ${this.glifAnimationState !== GlifAnimationState.INELIGIBLE ? html`
    <div id="glowWrapper" class="glow-container">
      ${entrypointButton}
      <div class="aim-gradient-outer-blur aim-c"></div>
      <div class="aim-gradient-solid aim-c"></div>
      <div class="aim-background aim-c"
        @animationend="${this.showContextMenuDescription
          ? nothing
          : (e: AnimationEvent) => {
              this.onAnimationEnd_(e, 'background-fade');
            }
        }"></div>
    </div>
    ` : entrypointButton}

  <cr-composebox-contextual-action-menu id="menu"
      .fileNum="${this.fileNum}"
      .disabledTabIds="${this.disabledTabIds}"
      .tabSuggestions="${this.tabSuggestions}"
      .entrypointName="${this.entrypointName}"
      .searchboxLayoutMode="${this.searchboxLayoutMode}"
      @close="${this.onMenuClose_}">
  </cr-composebox-contextual-action-menu>
<!--_html_template_end_-->`;
  // clang-format off
}

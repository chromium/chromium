// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {GlifAnimationState} from './common.js';
import type {ContextualEntrypointButtonElement} from './contextual_entrypoint_button.js';

export function getHtml(this: ContextualEntrypointButtonElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="${this.getWrapperId_()}" class="${this.getWrapperCssClass_()}">
    ${(this.showContextMenuDescription || this.showSuggestionLabel)
        && !this.windowWidthBelowThreshold_ ? html`
      <cr-button id="entrypoint" class="ai-mode-button" part="entrypoint-button"
          @click="${this.onEntrypointClick_}"
          title="${this.i18n('addContextTitle')}"
          ?disabled="${this.uploadButtonDisabled}" noink
          aria-label="${this.i18n('addContextTitle')}">
        <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
        <span id="description"
            @animationend="${this.onDescriptionAnimationend_}">
          ${this.showSuggestionLabel ?
             this.i18n('searchBoxHintMultimodal') : this.i18n('addContext')}
        </span>
        ${this.tabFaviconChipsToCoinsEnabled_ && this.sharedTabs && this.sharedTabs.length > 0 ? html`
          <composebox-favicon-group .tabs="${this.sharedTabs}" title="${this.i18n('sharingTabsWithGoogle')}"></composebox-favicon-group>
        ` : ''}
      </cr-button>
    ` : (this.tabFaviconChipsToCoinsEnabled_ && this.sharedTabs && this.sharedTabs.length > 0) ? html`
      <cr-button id="entrypoint" class="ai-mode-button pill-button" part="entrypoint-button"
          @click="${this.onEntrypointClick_}"
          title="${this.i18n('addContextTitle')}"
          ?disabled="${this.uploadButtonDisabled}" noink
          aria-label="${this.i18n('addContextTitle')}">
        <cr-icon id="entrypointIcon" icon="cr:add" slot="prefix-icon"></cr-icon>
        <composebox-favicon-group .tabs="${this.sharedTabs}" title="${this.i18n('sharingTabsWithGoogle')}"></composebox-favicon-group>
      </cr-button>
    ` : html`
      <cr-icon-button id="entrypoint" class="ai-mode-button"
          part="context-menu-entrypoint-icon entrypoint-button"
          iron-icon="cr:add"
          @click="${this.onEntrypointClick_}"
          title="${this.i18n('addContextTitle')}"
          ?disabled="${this.uploadButtonDisabled}" noink
          aria-label="${this.i18n('addContextTitle')}">
      </cr-icon-button>
    `}
    ${this.glifAnimationState !== GlifAnimationState.INELIGIBLE ? html`
      <div class="aim-gradient-outer-blur aim-c"></div>
      <div class="aim-gradient-solid aim-c"></div>
      <div class="aim-background aim-c"
          @animationend="${this.onAimBackgroundAnimationend_}">
      </div>
   ` : ''}
  </div>
<!--_html_template_end_-->`;
  // clang-format off
}

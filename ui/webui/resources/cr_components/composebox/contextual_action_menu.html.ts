// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import type {ContextualActionMenuElement} from './contextual_action_menu.js';

export function getHtml(this: ContextualActionMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <!-- auto-reposition is used to enable the ResizeObserver in cr-action-menu,
       which recalculates the menu's position when its content size changes.
       This is necessary because menu options (e.g. models, tabs) are populated
       asynchronously from the browser process. -->
  <cr-action-menu id="menu" role-description="${this.i18n('menu')}"
      @close="${this.onMenuClose_}" auto-reposition>
    ${this.tabSuggestions?.length > 0 && this.browserTabAllowed_ ? html`
      ${this.showContextMenuHeaders_ ? html`<h4 id="tabHeader">${
          this.getInputTypeLabel_(InputType.kBrowserTab)}</h4>` : ''}
      ${this.tabSuggestions.map((tab, index) => html`
        <div class="suggestion-container">
          <button class="dropdown-item"
              role="${this.enableMultiTabSelection_ ? 'menuitemcheckbox' : 'menuitem'}"
              aria-checked="${this.enableMultiTabSelection_ && this.disabledTabIds.has(tab.tabId)}"
              title="${tab.title}" data-index="${index}"
              aria-label="${this.getInputTypeLabel_(InputType.kBrowserTab)}, ${
                  tab.title}"
              ?disabled="${this.isTabDisabled_(tab)}"
              @pointerenter="${this.onTabPointerenter_}"
              @click="${this.onTabClick_}">
            <cr-composebox-tab-favicon .url="${tab.url}">
            </cr-composebox-tab-favicon>
            <span class="tab-title">${tab.title}</span>
            ${this.enableMultiTabSelection_ ? html`
              ${this.disabledTabIds.has(tab.tabId) ? html`
                <cr-icon class="multi-tab-icon"
                    icon="composebox:checkCircle" id="multi-tab-check"></cr-icon>
              ` : html`
                <cr-icon class="multi-tab-icon"
                    icon="composebox:addCircle" id="multi-tab-add"></cr-icon>
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
    ${this.imageUploadAllowed_ ? html`
      <button id="imageUpload" class="dropdown-item" role="menuitem"
          @click="${this.openImageUpload_}"
          ?disabled="${this.imageUploadDisabled_}">
        <cr-icon icon="composebox:imageUpload"></cr-icon>
        ${this.getInputTypeLabel_(InputType.kLensImage)}
      </button>` : ''}
    ${this.fileUploadAllowed_ ? html`<button id="fileUpload" class="dropdown-item"
        role="menuitem"
        @click="${this.openFileUpload_}"
        ?disabled="${this.fileUploadDisabled_}">
      <cr-icon icon="composebox:fileUpload"></cr-icon>
      ${this.getInputTypeLabel_(InputType.kLensFile)}
    </button>`: ''}

    <!-- Show a separator if there are tools AND (something above is visible) -->
    ${(this.inputState?.allowedTools.length ?? 0) > 0 &&
        (this.imageUploadAllowed_ || this.fileUploadAllowed_) ?
        html`<hr/>` : ''}

    ${(this.inputState?.allowedTools.length ?? 0) > 0 ? html`
        ${this.showContextMenuHeaders_ && this.toolHeader_ ? html`
        <h4 id="toolHeader">${this.toolHeader_}</h4>` : ''}` : ''}

    ${this.inputState?.allowedTools.map(mode => {
      return html`
      <button class="dropdown-item" data-mode="${mode}"
          role="menuitem"
          @click="${this.onToolClick_}"
          ?disabled="${this.isToolDisabled_(mode)}">
        ${this.getIconForToolMode_(mode) ? html`
          <cr-icon icon="${this.getIconForToolMode_(mode)}"></cr-icon>
        ` : ''}
        ${this.getToolLabel_(mode)}
      </button>`;
    })}

    <!-- Show a separator if there are models AND (something above is visible) -->
    ${(this.inputState?.allowedModels.length ?? 0) > 0 &&
      ((this.inputState?.allowedTools.length ?? 0) > 0 ||
       this.imageUploadAllowed_ || this.fileUploadAllowed_) ? html`<hr/>` : ''}

    ${(this.inputState?.allowedModels.length ?? 0) > 0 ? html`
        ${this.showContextMenuHeaders_ && this.modelHeader_ ? html`
        <h4 id="modelHeader">${this.modelHeader_}</h4>` : ''}` : ''}

    ${this.inputState?.allowedModels.map(mode => {
      return html`
      <button class="dropdown-item"
          role="menuitemradio"
          aria-checked="${this.isModelActive_(mode)}"
          data-model="${mode}"
          @click="${this.onModelClick_}"
          ?disabled="${this.isModelDisabled_(mode)}">
        ${this.getIconForModelMode_(mode) ? html`
          <cr-icon icon="${this.getIconForModelMode_(mode)}"></cr-icon>
        ` : ''}
        <span>${this.getModelLabel_(mode)}</span>
        ${this.isModelActive_(mode) ? html`
          <cr-icon class="multi-tab-icon"
              icon="cr:check" id="model-check"></cr-icon>` : ''}
      </button>`;
    })}
  </cr-action-menu>
<!--_html_template_end_-->`;
  // clang-format on
}

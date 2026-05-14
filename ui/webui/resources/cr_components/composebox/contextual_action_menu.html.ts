// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import type {ContextualActionMenuElement} from './contextual_action_menu.js';

export function getHtml(this: ContextualActionMenuElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <cr-action-menu id="menu" role-description="${this.i18n('menu')}"
      @close="${this.onMenuClose_}"
      ?auto-reposition="${!this.disableAutoReposition}">
    ${this.smartTabSharingVisible_ ? html`
      <button class="dropdown-item toggle-item"
          id="smartTabSharingItem"
          role="menuitem"
          @click="${this.onSmartTabSharingItemClick_}">
        <span>${this.i18n('stsMegaplusShareRelevantOpenTabs')}</span>
        <cr-toggle id="smartTabSharingToggle"
            ?checked="${this.smartTabSharingActive}"
            @change="${this.onSmartTabSharingToggleChange_}">
        </cr-toggle>
      </button>
      <hr/>
    ` : ''}
    ${this.tabSuggestions?.length > 0 && this.isBrowserTabAllowed_() ? html`
      ${this.contextManagementInComposeboxEnabled_ ? html`
        <div class="share-tabs-container">
          <button id="shareTabsTrigger" class="dropdown-item"
              role="menuitem"
              aria-popup="menu"
              aria-expanded="${this.shareTabsFlyoutOpen_}"
              @pointerenter="${this.onShareTabsRowPointerenter_}"
              @pointerleave="${this.onShareTabsRowPointerleave_}">
            <cr-icon icon="composebox:shareTabs"></cr-icon>
            <span class="tab-title">${this.i18n('shareTabs')}</span>
            <cr-icon class="share-tabs-arrow" icon="cr:chevron_right"></cr-icon>
          </button>
          <div class="share-tabs-flyout" role="menu"
              ?hidden="${!this.shareTabsFlyoutOpen_}"
              @pointerenter="${this.onShareTabsFlyoutPointerenter_}"
              @pointerleave="${this.onShareTabsFlyoutPointerleave_}">
            ${this.tabSuggestions.map((tab, index) => html`
              <div class="suggestion-container">
                <button class="dropdown-item"
                    role="${this.isMultiTabSelectionEnabledForShareTabsMode_() ?
                        'menuitemcheckbox' : 'menuitem'}"
                    aria-checked="${this.isMultiTabSelectionEnabledForShareTabsMode_() &&
                        this.disabledTabIds.has(tab.tabId)}"
                    title="${tab.title}" data-index="${index}"
                    aria-label="${this.getInputTypeLabel_(InputType.kBrowserTab)}: ${
                        tab.title}"
                    ?disabled="${this.isTabDisabled_(tab)}"
                    @click="${this.onTabClick_}">
                <cr-composebox-tab-favicon .url="${tab.url}">
                </cr-composebox-tab-favicon>
                <span class="tab-title-group">
                  <span class="tab-title">${tab.title}</span>
                  ${index === 0 ? html`
                    <span class="recent-tabs-suffix">${
                        this.i18n('recentTabsSuffix')}</span>
                  ` : ''}
                </span>
                ${this.isMultiTabSelectionEnabledForShareTabsMode_() &&
                    this.disabledTabIds.has(tab.tabId) ? html`
                  <cr-icon class="share-tabs-check" icon="cr:check"></cr-icon>
                ` : ''}
                </button>
              </div>
            `)}
          </div>
        </div>
        <hr/>
      ` : html`
        ${this.showContextMenuHeaders_ ? html`<h4 id="tabHeader">${
            this.getInputTypeLabel_(InputType.kBrowserTab)}</h4>` : ''}
        ${this.tabSuggestions.map((tab, index) => html`
          <div class="suggestion-container">
            <button class="dropdown-item"
                role="${this.enableMultiTabSelection_ ? 'menuitemcheckbox' : 'menuitem'}"
                aria-checked="${this.enableMultiTabSelection_ && this.disabledTabIds.has(tab.tabId)}"
                title="${tab.title}" data-index="${index}"
                aria-label="${this.getInputTypeLabel_(InputType.kBrowserTab)}: ${
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
      `}
    `: ''}
    ${this.isImageUploadAllowed_() ? html`
      <button id="imageUpload" class="dropdown-item" role="menuitem"
          @click="${this.onImageUploadClick_}"
          ?disabled="${this.isImageUploadDisabled_()}">
        <cr-icon icon="composebox:imageUpload"></cr-icon>
        ${this.getInputTypeLabel_(InputType.kLensImage)}
      </button>` : ''}
    ${this.isFileUploadAllowed_() ? html`<button id="fileUpload" class="dropdown-item"
        role="menuitem"
        @click="${this.onFileUploadClick_}"
        ?disabled="${this.isFileUploadDisabled_()}">
      <cr-icon icon="composebox:fileUpload"></cr-icon>
      ${this.getInputTypeLabel_(InputType.kLensFile)}
    </button>`: ''}
    ${this.isDriveUploadAllowed_() ? html`
      <button id="driveUpload" class="dropdown-item" role="menuitem"
          @click="${this.onDriveUploadClick_}"
          ?disabled="${this.isDriveUploadDisabled_()}">
        <cr-icon icon="composebox:driveUpload"></cr-icon>
        ${this.getInputTypeLabel_(InputType.kDrive)}
      </button>` : ''}

    <!-- Show a separator if there are tools AND (something above is visible) -->
    ${(this.inputState?.allowedTools.length ?? 0) > 0 &&
        (this.isImageUploadAllowed_() || this.isFileUploadAllowed_() ||
         this.isDriveUploadAllowed_()) ?
        html`<hr/>` : ''}

    ${(this.inputState?.allowedTools.length ?? 0) > 0 ? html`
        ${this.showContextMenuHeaders_ && this.getToolHeader_() ? html`
        <h4 id="toolHeader">${this.getToolHeader_()}</h4>` : ''}` : ''}

    ${this.inputState?.allowedTools.map(mode => {
      return html`
      <button class="dropdown-item" data-mode="${mode}"
          role="menuitem"
          aria-label="${this.showContextMenuHeaders_ && this.getToolHeader_() ?
              `${this.getToolHeader_()}: ` : ''}${this.getToolLabel_(mode)}"
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
       this.isImageUploadAllowed_() || this.isFileUploadAllowed_() ||
       this.isDriveUploadAllowed_()) ? html`<hr/>` : ''}

    ${(this.inputState?.allowedModels.length ?? 0) > 0 ? html`
        ${this.showContextMenuHeaders_ && this.getModelHeader_() ? html`
        <h4 id="modelHeader">${this.getModelHeader_()}</h4>` : ''}` : ''}

    ${this.inputState?.allowedModels.map(mode => {
      return html`
      <button class="dropdown-item"
          role="menuitemradio"
          aria-checked="${this.isModelActive_(mode)}"
          aria-label="${this.showContextMenuHeaders_ && this.getModelHeader_() ?
              `${this.getModelHeader_()}: ` : ''}${this.getModelLabel_(mode)}"
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

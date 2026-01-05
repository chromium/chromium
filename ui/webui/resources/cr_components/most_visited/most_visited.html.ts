// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MostVisitedElement} from './most_visited.js';

export function getHtml(this: MostVisitedElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" ?hidden="${!this.visible_}"
    .style="--tile-background-color: ${this.getBackgroundColorStyle_()};
            --column-count: ${this.columnCount_};
            --row-count: ${this.rowCount_};">
  ${this.tiles_.map((item, index) => html`
    <div class="tile" ?query-tile="${item.isQueryTile}"
      ?hidden="${this.isHidden_(index)}"
      title="${item.title}" @dragstart="${this.onDragStart_}"
      @touchstart="${this.onTouchStart_}" @click="${this.onTileClick_}"
      @mouseenter="${this.onTileHover_}" @mouseleave="${this.onTileExit_}"
      @mousedown="${this.onTileMouseDown_}" @keydown="${this.onTileKeyDown_}"
      draggable="true" data-index="${index}">
      <a href="${item.url.url}" aria-label="${item.title}"
          draggable="false">
      </a>
      <cr-icon-button id="actionMenuButton" class="icon-more-vert"
          title="${this.getMoreActionText_(item.title)}"
          @click="${this.onTileActionButtonClick_}" tabindex="0"
          ?hidden="${!this.customLinksEnabled_ &&
            !this.isFromEnterpriseShortcut_(item.source)}"
          data-index="${index}"></cr-icon-button>
      <cr-icon-button id="removeButton" class="icon-clear"
          title="${this.i18n('linkRemoveA11y', item.title)}"
          @click="${this.onTileRemoveButtonClick_}" tabindex="0"
          ?hidden="${this.customLinksEnabled_ ||
            this.isFromEnterpriseShortcut_(item.source)}"
          data-index="${index}"></cr-icon-button>
      <div class="tile-icon">
        <img src="${this.getFaviconUrl_(item.url)}" draggable="false"
            ?hidden="${item.isQueryTile}" alt=""></img>
        <div class="query-tile-icon" draggable="false"
            ?hidden="${!item.isQueryTile}"></div>
        <div class="managed-tile-icon"
          ?hidden="${!this.isFromEnterpriseShortcut_(item.source)}">
          <cr-policy-indicator indicator-type="userPolicy">
          </cr-policy-indicator>
        </div>
      </div>
      <div class="tile-title ${this.getTileTitleDirectionClass_(item)}">
        <span>${item.title}</span>
      </div>
    </div>
  `)}
  <cr-button id="addShortcut" tabindex="0" @click="${this.onAdd_}"
      ?hidden="${!this.showAdd_}" @keydown="${this.onAddShortcutKeyDown_}"
      aria-label="${this.i18n('addLinkTitle')}"
      title="${this.i18n('addLinkTitle')}" noink>
    <div class="tile-icon tile-icon-container">
      <div id="addShortcutIcon" draggable="false"></div>
    </div>
    <div class="tile-title">
      <span>${this.i18n('addLinkTitle')}</span>
    </div>
  </cr-button>
  <div>
    <cr-button id="showMore" tabindex="0" @click="${this.onShowMoreClick_}"
        ?hidden="${!this.showShowMore_}" @keydown="${this.onShowMoreKeyDown_}"
        aria-label="${this.i18n('showMore')}"
        title="${this.i18n('showMore')}" noink>
      <div class="tile-icon tile-icon-container">
        <div id="showMoreIcon" draggable="false"></div>
      </div>
      <div class="tile-title">
        <span>${this.i18n('showMore')}</span>
      </div>
    </cr-button>
    <cr-button id="showLess" tabindex="0" @click="${this.onShowLessClick_}"
        ?hidden="${!this.showShowLess_}" @keydown="${this.onShowLessKeyDown_}"
        aria-label="${this.i18n('showLess')}"
        title="${this.i18n('showLess')}" noink>
      <div class="tile-icon tile-icon-container">
        <div id="showLessIcon" draggable="false"></div>
      </div>
      <div class="tile-title">
        <span>${this.i18n('showLess')}</span>
      </div>
    </cr-button>
  </div>
  <cr-dialog id="dialog" @close="${this.onDialogClose_}">
    <div slot="title">${this.dialogTitle_}</div>
    <div slot="body" id="dialogContent">
      ${this.isFromEnterpriseShortcut_(this.dialogSource_) ? html`
        <div id="policySubtitleContainer">
          <cr-icon icon="cr:domain"></cr-icon>
          <span class="secondary">
            ${this.i18n('enterpriseShortcutSubtitle')}
          </span>
        </div>` : ''}
      <cr-input id="dialogInputName" label="${this.i18n('nameField')}"
          .value="${this.dialogTileTitle_}"
          ?readonly="${this.dialogIsReadonly_}"
          spellcheck="false" autofocus
          @value-changed="${this.onDialogTileNameChange_}">
      </cr-input>
      <cr-input id="dialogInputUrl" label="${this.i18n('urlField')}"
          .value="${this.dialogTileUrl_}"
          ?invalid="${this.dialogTileUrlInvalid_}"
          .errorMessage="${this.dialogTileUrlError_}" spellcheck="false"
          type="url" @blur="${this.onDialogTileUrlBlur_}"
          @value-changed="${this.onDialogTileUrlChange_}"
          ?readonly="${this.dialogIsReadonly_ ||
            this.isFromEnterpriseShortcut_(this.dialogSource_)}">
      </cr-input>
    </div>
    <div slot="button-container">
      <cr-button class="cancel-button" @click="${this.onDialogCancel_}"
          ?hidden="${this.dialogIsReadonly_}">
        ${this.i18n('linkCancel')}
      </cr-button>
      <cr-button class="action-button" @click="${this.onSave_}"
          ?disabled="${this.dialogSaveDisabled_}">
        ${this.i18n('linkDone')}
      </cr-button>
    </div>
  </cr-dialog>
  <cr-action-menu id="actionMenu">
    <button id="actionMenuViewOrEdit" class="dropdown-item"
        @click="${this.onViewOrEdit_}">
      ${this.actionMenuViewOrEditTitle_}
    </button>
    <button id="actionMenuRemove" class="dropdown-item"
        @click="${this.onRemove_}"
        ?disabled="${this.actionMenuRemoveDisabled_}">
      ${this.i18n('linkRemove')}
    </button>
  </cr-action-menu>
</div>
<cr-toast-manager id="toastManager" duration="10000">
  <cr-button id="undo" aria-label="${this.i18n('undoDescription')}"
      @click="${this.onUndoClick_}">
    ${this.i18n('undo')}
  </cr-button>
  <cr-button id="restore"
      aria-label="${this.getRestoreButtonText_()}"
      @click="${this.onRestoreDefaultsClick_}">
    ${this.getRestoreButtonText_()}
  </cr-button>
</cr-toast-manager>
<!--_html_template_end_-->`;
  // clang-format on
}

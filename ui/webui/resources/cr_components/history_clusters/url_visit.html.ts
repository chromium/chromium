// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {UrlVisitElement} from './url_visit.js';

export function getHtml(this: UrlVisitElement) {
  return html`
<div id="header" @click="${this.onClick_}" @auxclick="${this.onClick_}"
    @keydown="${this.onKeydown_}" @contextmenu="${this.onContextMenu_}">
  <a id="link-container" href="${this.visit?.normalizedUrl.url || nothing}">
    <page-favicon id="icon" .url="${this.visit?.normalizedUrl}"
        .isKnownToSync="${this.visit?.isKnownToSync || false}">
    </page-favicon>
    <div id="page-info">
      <div id="title-and-annotations">
        <span id="title" class="truncate"></span>
        ${this.computeAnnotations_().map(
            item => html`<span class="annotation">${item}</span>`)}
      </div>
      <span id="url" class="truncate"></span>
      <span id="debug-info" ?hidden="${!this.computeDebugInfo_()}">
        ${this.computeDebugInfo_()}
      </span>
    </div>
  </a>
  <div class="suffix-icons">
    <cr-icon-button class="hide-visit-icon"
        title="${this.i18n('hideFromCluster')}"
        @click="${this.onHideSelfButtonClick_}"
        ?hidden="${!this.fromPersistence}"></cr-icon-button>
    <cr-icon-button id="actionMenuButton" class="icon-more-vert"
        title="${this.i18n('actionMenuDescription')}" aria-haspopup="menu"
        @click="${this.onActionMenuButtonClick_}"
        ?hidden="${!this.allowDeletingHistory_}">
    </cr-icon-button>
  </div>
</div>

${this.renderActionMenu_ ? html`
    <cr-action-menu role-description="${this.i18n('actionMenuDescription')}">
      <button id="removeSelfButton" class="dropdown-item"
          ?hidden="${!this.allowDeletingHistory_}"
          @click="${this.onRemoveSelfButtonClick_}">
        ${this.i18n('removeFromHistory')}
      </button>
    </cr-action-menu>` : ''}`;
}

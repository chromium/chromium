// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ClusterMenuElement} from './cluster_menu.js';

export function getHtml(this: ClusterMenuElement) {
  return html`
<cr-icon-button id="actionMenuButton" class="icon-more-vert"
    title="${this.i18n('actionMenuDescription')}" aria-haspopup="menu"
    @click="${this.onActionMenuButtonClick_}">
</cr-icon-button>

${this.renderActionMenu_ ? html`<cr-action-menu
    role-description="${this.i18n('actionMenuDescription')}">
  <button id="openAllButton" class="dropdown-item"
      @click="${this.onOpenAllButtonClick_}">
    ${this.i18n('openAllInTabGroup')}
  </button>
  <button id="hideAllButton" class="dropdown-item"
      @click="${this.onHideAllButtonClick_}">
    ${this.i18n('hideAllVisits')}
  </button>
  <button id="removeAllButton" class="dropdown-item"
      @click="${this.onRemoveAllButtonClick_}"
      ?hidden="${!this.allowDeletingHistory_}">
    ${this.i18n('removeAllFromHistory')}
  </button>
</cr-action-menu>` : ''}`;
}

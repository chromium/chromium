// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryClustersElement} from './clusters.js';

// clang-format off
export function getHtml(this: HistoryClustersElement) {
  return html`
<div id="placeholder" ?hidden="${!this.computePlaceholderText_()}">
  ${this.computePlaceholderText_()}
</div>
<cr-infinite-list id="clusters"
    .items="${this.clusters_}"
    @hide-visit="${this.onHideVisit_}" @hide-visits="${this.onHideVisits_}"
    @remove-visits="${this.onRemoveVisits_}"
    ?hidden="${!this.clusters_.length}" .scrollTarget="${this.scrollTarget}"
    .scrollOffset="${this.scrollOffset}"
    .template=${(item: any, index: number, tabindex: number) => html`
      <history-cluster .cluster="${item}" .index="${index}"
          .query="${this.resultQuery_}" tabindex="${tabindex}"
          @remove-cluster="${this.onRemoveCluster_}" ?is-first="${!index}"
          ?is-last="${this.isLastCluster_(index)}">
      </history-cluster>`}>
</cr-infinite-list>
<div id="footer" ?hidden="${this.getLoadMoreButtonHidden_()}">
  <cr-button id="loadMoreButton" @click="${this.onLoadMoreButtonClick_}"
      ?hidden="${this.showSpinner_}">
    ${this.i18n('loadMoreButtonLabel')}
  </cr-button>
  <img class="spinner-icon" src="chrome://resources/images/throbber_small.svg"
      ?hidden="${!this.showSpinner_}"></img>
</div>
</iron-scroll-threshold>
${this.showConfirmationDialog_ ? html`<cr-dialog consume-keydown-event
    @cancel="${this.onConfirmationDialogCancel_}">
      <div slot="title">${this.i18n('removeSelected')}</div>
      <div slot="body">${this.i18n('deleteWarning')}</div>
      <div slot="button-container">
        <cr-button class="cancel-button" @click="${this.onCancelButtonClick_}">
          ${this.i18n('cancel')}
        </cr-button>
        <cr-button class="action-button" @click="${this.onRemoveButtonClick_}">
          ${this.i18n('deleteConfirm')}
        </cr-button>
      </div>
    </cr-dialog>` : ''}
<cr-toast id="confirmationToast" duration="5000">
  <div>${this.i18n('removeFromHistoryToast')}</div>
</cr-toast>`;
}

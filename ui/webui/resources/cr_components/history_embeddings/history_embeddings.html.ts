// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryEmbeddingsElement} from './history_embeddings.js';

export function getHtml(this: HistoryEmbeddingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${!this.enableAnswers_ ? html`
  <div id="cardWithoutAnswers" class="card">
    <h2 class="heading results-heading">
      <cr-icon icon="history-embeddings:heading"></cr-icon>
      ${this.getHeadingText_()}
    </h2>

    ${this.loadingResults_ ? html`
      <div class="loading loading-results">
        <cr-loading-gradient>
          <svg width="482" height="40">
            <clipPath>
              <rect width="40" height="40" rx="8" ry="8"></rect>
              <rect x="55" y="4" width="calc(100% - 55px)" height="14" rx="4" ry="4"></rect>
              <rect x="55" y="24" width="calc(78% - 55px)" height="14" rx="4" ry="4"></rect>
            </clipPath>
          </svg>
        </cr-loading-gradient>
      </div>
    ` : html`
      <div>
        <div class="result-items">
          ${this.searchResult_?.items.map((item, index) => html`
            <cr-url-list-item url="${item.url.url}" title="${item.title}"
                description="${item.urlForDisplay}"
                @click="${this.onResultClick_}" @auxclick="${this.onResultClick_}"
                data-index="${index}" @contextmenu="${this.onResultContextMenu_}"
                as-anchor as-anchor-target="_blank" always-show-suffix>
              <span class="time" slot="suffix">${this.getDateTime_(item)}</span>
              <cr-icon-button slot="suffix" iron-icon="cr:more-vert"
                  data-index="${index}" @click="${this.onMoreActionsClick_}"
                  aria-label="${this.i18n('actionMenuDescription')}"
                  aria-description="${item.title}">
              </cr-icon-button>
              <div slot="footer" class="source-passage"
                  ?hidden="${!item.sourcePassage}">
                <div class="source-passage-line"></div>
                ${item.sourcePassage}
              </div>
            </cr-url-list-item>
            <hr>
          `)}
        </div>

        <div class="footer">
          <div>${this.i18n('historyEmbeddingsFooter')}</div>
          <cr-feedback-buttons
              selected-option="${this.feedbackState_}"
              @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}">
          </cr-feedback-buttons>
        </div>
      </div>
    `}
  </div>
` : ''}

${this.enableAnswers_ ? html`
  <div id="cardWithAnswers" class="card">
    ${this.showAnswerSection_() ? html`
      <div class="answer-section">
        <h2 class="heading">${this.getHeadingTextForAnswerSection_()}</h2>
        ${this.loadingAnswer_ ? html`
          <div class="loading loading-answer">
            <cr-loading-gradient>
              <svg width="100%" height="72" fill="none" xmlns="http://www.w3.org/2000/svg">
                <clipPath>
                  <rect width="100%" height="12" rx="4"></rect>
                  <rect y="20" width="85%" height="12" rx="4"></rect>
                  <rect y="40" width="75%" height="12" rx="4"></rect>
                  <rect y="60" width="33%" height="12" rx="4"></rect>
                </clipPath>
              </svg>
            </cr-loading-gradient>
          </div>
        ` : html`
          <div class="answer" ?is-error="${this.isAnswerErrorState_()}">
            ${this.getAnswerOrError_()}
          </div>
        `}
        ${this.answerSource_ ? html`
          <div class="answer-source">
            <a class="answer-link"
                href="${this.getAnswerSourceUrl_()}" target="_blank"
                @click="${this.onAnswerLinkClick_}"
                @auxclick="${this.onAnswerLinkClick_}"
                @contextmenu="${this.onAnswerLinkContextMenu_}">
              <div class="favicon"
                  .style="background-image: ${this.getFavicon_(this.answerSource_)}"></div>
              <div class="result-url">${this.answerSource_.urlForDisplay}</div>
            </a>
            &bull;
            <div class="time">${this.getAnswerDateTime_()}</div>
          </div>
        ` : ''}
      </div>
    ` : ''}

    <h2 class="heading results-heading">${this.getHeadingText_()}</h2>
    ${this.loadingResults_ ? html`
      <div class="loading loading-results">
        <cr-loading-gradient>
          <svg width="100%" height="40">
            <clipPath>
              <rect width="40" height="40" rx="8" ry="8"></rect>
              <rect x="55" y="4" width="calc(100% - 55px)" height="14" rx="4" ry="4"></rect>
              <rect x="55" y="24" width="calc(78% - 55px)" height="14" rx="4" ry="4"></rect>
            </clipPath>
          </svg>
        </cr-loading-gradient>
      </div>
    ` : html`
      <div class="result-items">
        ${this.searchResult_?.items.map((item, index) => html`
          ${this.enableImages_ ? html`
            <a class="result-item" href="${item.url.url}" target="_blank"
                @click="${this.onResultClick_}" @auxclick="${this.onResultClick_}"
                data-index="${index}" @contextmenu="${this.onResultContextMenu_}">
              <div class="result-image">
                <cr-history-embeddings-result-image
                    ?in-side-panel="${this.inSidePanel}"
                    .searchResult="${item}">
                </cr-history-embeddings-result-image>
                <div class="favicon"
                    .style="background-image: ${this.getFavicon_(item)}"></div>
              </div>
              <div class="result-metadata">
                <div class="result-title">${item.title}</div>
                <div class="result-url-and-favicon">
                  <div class="favicon"
                      .style="background-image: ${this.getFavicon_(item)}"></div>
                  <div class="result-url">${item.urlForDisplay}</div>
                </div>
              </div>
              <span class="time">${this.getDateTime_(item)}</span>
              <cr-icon-button class="more-actions" iron-icon="cr:more-vert"
                  data-index="${index}" @click="${this.onMoreActionsClick_}"
                  aria-label="${this.i18n('actionMenuDescription')}"
                  aria-description="${item.title}">
              </cr-icon-button>
            </a>
          ` : html`
            <cr-url-list-item url="${item.url.url}" title="${item.title}"
                description="${item.urlForDisplay}"
                @click="${this.onResultClick_}" @auxclick="${this.onResultClick_}"
                @contextmenu="${this.onResultContextMenu_}"
                data-index="${index}"
                as-anchor as-anchor-target="_blank" always-show-suffix>
              <span class="time" slot="suffix">${this.getDateTime_(item)}</span>
              <cr-icon-button slot="suffix" iron-icon="cr:more-vert"
                  data-index="${index}" @click="${this.onMoreActionsClick_}"
                  aria-label="${this.i18n('actionMenuDescription')}"
                  aria-description="${item.title}">
              </cr-icon-button>
              <div slot="footer" class="source-passage"
                  ?hidden="${!item.sourcePassage}">
                <div class="source-passage-line"></div>
                ${item.sourcePassage}
              </div>
            </cr-url-list-item>
            <hr>
          `}
        `)}
      </div>
    `}
  </div>
  <div class="footer">
    <div>${this.i18n('historyEmbeddingsFooter')}</div>
    <cr-feedback-buttons
        selected-option="${this.feedbackState_}"
        @selected-option-changed="${this.onFeedbackSelectedOptionChanged_}">
    </cr-feedback-buttons>
  </div>
` : ''}

<cr-lazy-render-lit id="sharedMenu" .template="${() => html`
  <cr-action-menu role-description="${this.i18n('actionMenuDescription')}">
    ${this.showMoreFromSiteMenuOption ? html`
      <button id="moreFromSiteOption" class="dropdown-item"
          @click="${this.onMoreFromSiteClick_}">
        ${this.i18n('moreFromSite')}
      </button>
    ` : ''}
    <button id="removeFromHistoryOption"
        class="dropdown-item" @click="${this.onRemoveFromHistoryClick_}">
      ${this.i18n('removeFromHistory')}
    </button>
  </cr-action-menu>`}">
</cr-lazy-render-lit>
<!--_html_template_end_-->`;
  // clang-format on
}

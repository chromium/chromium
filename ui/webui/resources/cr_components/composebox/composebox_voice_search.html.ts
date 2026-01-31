// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';

export function getHtml(this: ComposeboxVoiceSearchElement) {
  return html`
    <div id="container">
      <div id="error-container" ?hidden="${!this.showErrorScrim_}">
        <span id="error-message">${this.errorMessage_}</span>
        <a id="details" target="_blank" href="${this.detailsUrl_}"
            @click="${this.onLinkClick_}">
          ${this.i18n('voiceDetails')}
        </a>
      </div>
      <textarea id="input"
          .value="${this.finalResult_ + (this.interimResult_ || '')}"
          placeholder="${this.listeningPlaceholder_}"
          ?hidden="${this.showErrorScrim_}" disabled>
      </textarea>
      <cr-icon-button id="closeButton" class="icon-clear"
          part="voice-close-button"
          title="${this.i18n('voiceClose')}" @click="${this.onCloseClick_}">
      </cr-icon-button>
    </div>
  `;
}

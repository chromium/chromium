// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';
import {VoiceSearchError} from './composebox_voice_search.js';

export function getHtml(this: ComposeboxVoiceSearchElement) {
  return html`
    <div id="container">
      <div id="error-container" ?hidden="${!this.shouldShowErrorScrim_()}">
        <span id="error-message">${this.errorMessage_}</span>
        ${this.detailedError_ === VoiceSearchError.NO_MATCH ?
          html`<a id="tryAgainLink" href="#" @click="${this.onTryAgainClick_}">
            ${this.i18n('tryAgain')}
          </a>
        `: ''}
        <a id="details" part="voice-details-link" target="_blank" href="${
      this.detailsUrl_}"
            @click="${this.onLinkClick_}">
          ${this.i18n('voiceDetails')}
        </a>
      </div>
      ${this.liveTranscriptEnabled ?
          html`<textarea id="input"
              .value="${this.transcript_}"
              placeholder="${this.listeningPlaceholder_}"
              ?hidden="${this.shouldShowErrorScrim_()}" disabled>
          </textarea>
          `: ''}
      ${!this.submitStopButtonsEnabled ||
          this.shouldShowErrorScrim_() ?
      html`<cr-icon-button id="closeButton" class="icon-clear"
              part="voice-close-button"
              title="${this.i18n('voiceClose')}" @click="${this.onCloseClick_}"
              ></cr-icon-button>
          ` : ''}
      ${
      this.submitStopButtonsEnabled ?
      html`<div id="bottomActions"
              ?hidden="${this.shouldShowErrorScrim_()}">
            <cr-icon-button id="stopButton" part="voice-stop-button"
                iron-icon="composebox:stop"
                 title="${this.i18n('voiceStop')}"
                @click="${this.onStopClick_}"
                >
            </cr-icon-button>
            <cr-composebox-submit id="submitButton"
                exportparts="submit"
                part="voice-submit-button"
                .iconType="${this.submitButtonIconType}"
                .submitButtonTitle="${this.i18n('composeboxSubmitButtonTitle')}"
                @submit-click="${this.onSubmitClick_}"
                ?disabled="${!(this.finalResult_ || this.interimResult_)}">
            </cr-composebox-submit>
          </div>
        ` : ''}
    </div>
  `;
}

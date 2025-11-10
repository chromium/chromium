// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxVoiceSearchElement} from './composebox_voice_search.js';

export function getHtml(this: ComposeboxVoiceSearchElement) {
  return html`
    <div id="container">
      <textarea id="input" .value="${this.finalResult_}" placeholder="${
          this.listeningPlaceholder_}"></textarea>
      <cr-icon-button id="closeButton" class="icon-clear"
          title="Close" @click="${this.onCloseClick_}">
      </cr-icon-button>
    </div>
  `;
}

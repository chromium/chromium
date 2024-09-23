// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrFeedbackButtonsElement} from './cr_feedback_buttons.js';

export function getHtml(this: CrFeedbackButtonsElement) {
  return html`
<div class="buttons">
  <cr-icon-button id="thumbsUp" iron-icon="${this.getThumbsUpIcon_()}"
      aria-label="${this.thumbsUpLabel_}"
      title="${this.thumbsUpLabel_}"
      aria-pressed="${this.getThumbsUpAriaPressed_()}"
      @click="${this.onThumbsUpClick_}"
      ?disabled="${this.disabled}">
  </cr-icon-button>
  <cr-icon-button id="thumbsDown"
      iron-icon="${this.getThumbsDownIcon_()}"
      aria-label="${this.thumbsDownLabel_}"
      title="${this.thumbsDownLabel_}"
      aria-pressed="${this.getThumbsDownAriaPressed_()}"
      @click="${this.onThumbsDownClick_}"
      ?disabled="${this.disabled}">
  </cr-icon-button>
</div>`;
}

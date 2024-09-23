// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToolbarSelectionOverlayElement} from './cr_toolbar_selection_overlay.js';

export function getHtml(this: CrToolbarSelectionOverlayElement) {
  return html`
<div id="overlay-content">
  <cr-icon-button part="clearIcon"
      title="${this.cancelLabel}" iron-icon="cr:clear"
      @click="${this.onClearSelectionClick_}"></cr-icon-button>
  <div id="number-selected">${this.selectionLabel}</div>
  <div id="slot"><slot></slot></div>
</div>`;
}

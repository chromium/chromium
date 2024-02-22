// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './history_embeddings.html.js';

export class HistoryEmbeddingsElement extends PolymerElement {
  static get is() {
    return 'cr-history-embeddings';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings': HistoryEmbeddingsElement;
  }
}

customElements.define(HistoryEmbeddingsElement.is, HistoryEmbeddingsElement);

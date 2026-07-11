// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '/resources/lit/v3_0/lit.rollup.js';

export class GenericListElement<T> extends CrLitElement {
  static get is() {
    return 'generic-list';
  }

  static override get properties() {
    return {
      items: {type: Array},
      selectedItem: {type: Object},
    };
  }

  accessor items: T[] = [];
  accessor selectedItem: T|undefined;
}

declare global {
  interface HTMLElementTagNameMap {
    'generic-list': GenericListElement<unknown>;
  }
}

customElements.define(GenericListElement.is, GenericListElement);

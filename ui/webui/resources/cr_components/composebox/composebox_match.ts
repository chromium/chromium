// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {getCss} from './composebox_match.css.js';
import {getHtml} from './composebox_match.html.js';

// Displays an autocomplete match
export class ComposeboxMatchElement extends CrLitElement {
  static get is() {
    return 'ntp-composebox-match';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      match: {type: Object},
    };
  }

  accessor match: AutocompleteMatch;

  constructor() {
    super();
  }

  private decodeString16_(str: String16|null): string {
    return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
  }

  protected computeContents_(): string {
    if (!this.match) {
      return '';
    }
    return this.decodeString16_(this.match.contents);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox-match': ComposeboxMatchElement;
  }
}

customElements.define(ComposeboxMatchElement.is, ComposeboxMatchElement);

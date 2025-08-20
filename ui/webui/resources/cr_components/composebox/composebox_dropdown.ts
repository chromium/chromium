// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox_match.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_dropdown.css.js';
import {getHtml} from './composebox_dropdown.html.js';

// TODO(crbug.com/439616869): Provide an API for the embedder (i.e., <cr-composebox>)
// to change the selection.
// A dropdown element that contains autocomplete matches.
export class ComposeboxDropdownElement extends CrLitElement {
  static get is() {
    return 'ntp-composebox-dropdown';
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

      result: {
        type: Object,
      },
    };
  }

  accessor result: AutocompleteResult|null = null;
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox-dropdown': ComposeboxDropdownElement;
  }
}

customElements.define(ComposeboxDropdownElement.is, ComposeboxDropdownElement);

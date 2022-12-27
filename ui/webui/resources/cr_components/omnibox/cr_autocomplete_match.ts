// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_autocomplete_match.html.js';
import {AutocompleteMatch} from './omnibox.mojom-webui.js';

/** Converts a String16 to a JavaScript String. */
function decodeString16(str: String16|null): string {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

/** @return A <span> with the given text as textContent. */
function renderText(text: string): Element {
  const span = document.createElement('span');
  span.textContent = text;
  return span;
}

// Displays an autocomplete match.
export class AutocompleteMatchElement extends PolymerElement {
  static get is() {
    return 'cr-autocomplete-match';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      match: {
        type: Object,
      },

      /** Rendered match contents. */
      contentsHtml_: {
        type: String,
        computed: `computeContentsHtml_(match)`,
      },

      /** Rendered match description. */
      descriptionHtml_: {
        type: String,
        computed: `computeDescriptionHtml_(match)`,
      },

      /** Used to separate the match content and the description. */
      separatorText_: {
        type: String,
        computed: `computeSeparatorText_(match)`,
      },
    };
  }

  match: AutocompleteMatch;
  private contentsHtml_: TrustedHTML;
  private descriptionHtml_: TrustedHTML;
  private separatorText_: string;

  private computeContentsHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    const contents = this.match.answer?.firstLine ?? this.match.contents;
    return sanitizeInnerHtml(renderText(decodeString16(contents)).innerHTML);
  }

  private computeDescriptionHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    const description = this.match.answer?.secondLine ?? this.match.description;
    return sanitizeInnerHtml(renderText(decodeString16(description)).innerHTML);
  }

  private computeSeparatorText_(): string {
    return this.match && decodeString16(this.match.description) ? ' - ' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-autocomplete-match': AutocompleteMatchElement;
  }
}

customElements.define(AutocompleteMatchElement.is, AutocompleteMatchElement);

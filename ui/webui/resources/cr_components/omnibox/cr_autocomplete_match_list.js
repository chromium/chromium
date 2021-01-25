// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Custom element for the omnibox popup used in the
 * WebUI NTP realbox and (experimentally) in the top chrome omnibox.
 */

/**
 * TODO(tommycli): Coalesce this with the Mojo AutocompleteMatch.
 * @typedef {{
 *   contents: string,
 *   description: string,
 * }}
 */
let AutocompleteMatch;

const staticHtmlPolicy = trustedTypes.createPolicy(
    'cr-autocomplete-match', {createHTML: () => `{__html_template__}`});

class AutocompleteMatchElement extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode: 'open'});
    this.shadowRoot.innerHTML = staticHtmlPolicy.createHTML('');
  }

  /** @param {!AutocompleteMatch} match */
  updateMatch(match) {
    const shadowRoot = this.shadowRoot;
    shadowRoot.getElementById('contents').textContent = match.contents;
    shadowRoot.getElementById('description').textContent = match.description;
  }
}

customElements.define('cr-autocomplete-match', AutocompleteMatchElement);

export class AutocompleteMatchListElement extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});

    // TODO(tommycli): Get this from the browser via loadTimeData.
    const maxAutocompleteElements = 8;
    for (let i = 0; i < maxAutocompleteElements; i++) {
      shadowRoot.appendChild(document.createElement('cr-autocomplete-match'));
    }
  }

  /** @param {!Array<!AutocompleteMatch>} matches */
  updateMatches(matches) {
    const shadowRoot = /** @type {!ParentNode} */ (this.shadowRoot);
    for (let i = 0; i < matches.length && i < shadowRoot.children.length; i++) {
      shadowRoot.children[i].updateMatch(matches[i]);
    }
  }
}

customElements.define(
    'cr-autocomplete-match-list', AutocompleteMatchListElement);

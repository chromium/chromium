// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_match.css.js';
import {getHtml} from './composebox_match.html.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';

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

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       */
      matchIndex: {type: Number},
    };
  }

  accessor match: AutocompleteMatch;

  accessor matchIndex: number;
  private searchboxHandler_: SearchboxPageHandlerRemote;

  constructor() {
    super();
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('click', (event) => this.onMatchClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());
  }

  protected computeContents_(): string {
    if (!this.match) {
      return '';
    }
    return mojoString16ToString(this.match.contents);
  }

  protected iconPath_(): string {
    return this.match.iconPath;
  }

  private onMatchFocusin_() {
    this.fire('match-focusin', {
      index: this.matchIndex,
    });
  }

  private onMatchClick_(e: MouseEvent) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    e.preventDefault();  // Prevents default browser action (navigation).

    this.searchboxHandler_.openAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl,
        /* are_matches_showing */ true, e.button || 0, e.altKey, e.ctrlKey,
        e.metaKey, e.shiftKey);

    this.fire('match-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-composebox-match': ComposeboxMatchElement;
  }
}

customElements.define(ComposeboxMatchElement.is, ComposeboxMatchElement);

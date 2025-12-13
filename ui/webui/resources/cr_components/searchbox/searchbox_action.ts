// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './searchbox_action.css.js';
import {getHtml} from './searchbox_action.html.js';

// Displays a UI chip for an `AutocompleteMatch`. E.g. for keywords ('Search
// YouTube') or actions ('Clear browsing data').
export class SearchboxActionElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-action';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hint: {type: String},

      hintHtml_: {
        state: true,
        type: String,
      },

      suggestionContents: {type: String},

      iconPath: {type: String},

      iconStyle_: {
        state: true,
        type: String,
      },

      ariaLabel: {type: String},

      // Index of the action in the autocomplete result. Used to inform handler
      // of action that was selected.
      actionIndex: {type: Number},
    };
  }

  accessor hint: string = '';
  protected accessor hintHtml_: TrustedHTML = window.trustedTypes!.emptyHTML;
  accessor suggestionContents: string = '';
  accessor iconPath: string = '';
  protected accessor iconStyle_: string = '';
  override accessor ariaLabel: string = '';
  accessor actionIndex: number = -1;

  override firstUpdated() {
    this.addEventListener('click', (event) => this.onActionClick_(event));
    this.addEventListener('keydown', (event) => this.onActionKeyDown_(event));
    this.addEventListener(
        'mousedown', (event) => this.onActionMouseDown_(event));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('hint')) {
      this.hintHtml_ = this.computeHintHtml_();
    }
    if (changedProperties.has('iconPath')) {
      this.iconStyle_ = this.computeActionIconStyle_();
    }
  }

  private onActionClick_(e: PointerEvent|KeyboardEvent) {
    this.fire('execute-action', {
      event: e,
      actionIndex: this.actionIndex,
    });

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onActionKeyDown_(e: KeyboardEvent) {
    if (e.key && (e.key === 'Enter' || e.key === ' ')) {
      this.onActionClick_(e);
    }
  }

  private onActionMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeHintHtml_(): TrustedHTML {
    if (this.hint) {
      return sanitizeInnerHtml(this.hint);
    }
    return window.trustedTypes!.emptyHTML;
  }

  private computeActionIconStyle_(): string {
    // If this is a custom icon, shouldn't follow the standard theming given to
    // all other action icons.
    if (this.iconPath.startsWith('data:image/')) {
      return `background-image: url(${this.iconPath})`;
    }

    return `-webkit-mask-image: url(${this.iconPath})`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-action': SearchboxActionElement;
  }
}

customElements.define(SearchboxActionElement.is, SearchboxActionElement);

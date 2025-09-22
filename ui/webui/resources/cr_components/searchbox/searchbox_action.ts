// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {Action} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './searchbox_action.css.js';
import {getHtml} from './searchbox_action.html.js';
import {decodeString16} from './utils.js';

// Displays an action associated with AutocompleteMatch (i.e. Clear
// Browsing History, etc.)
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
      //========================================================================
      // Public properties
      //========================================================================
      action: {type: Object},

      /**
       * Index of the action in the autocomplete result. Used to inform handler
       * of action that was selected.
       */
      actionIndex: {type: Number},

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as click, keyboard events etc.
       */
      matchIndex: {type: Number},

      //========================================================================
      // Private properties
      //========================================================================
      actionIconStyle_: {type: String},

      /** Element's 'aria-label' attribute. */
      ariaLabel: {
        type: String,
        reflect: true,
      },

      /** Rendered hint from action. */
      hintHtml_: {type: String},

      /** Rendered tooltip from action. */
      tooltip_: {type: String},
    };
  }

  accessor action: Action;
  accessor actionIndex: number = -1;
  accessor matchIndex: number = -1;
  override accessor ariaLabel: string;
  protected accessor hintHtml_: TrustedHTML;
  protected accessor tooltip_: string;
  protected accessor actionIconStyle_: string;

  override firstUpdated() {
    this.addEventListener('click', (event) => this.onActionClick_(event));
    this.addEventListener('keydown', (event) => this.onActionKeyDown_(event));
    this.addEventListener(
        'mousedown', (event) => this.onActionMouseDown_(event));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('action')) {
      this.actionIconStyle_ = this.computeActionIconStyle_();
      this.ariaLabel = this.computeAriaLabel_();
      this.hintHtml_ = this.computeHintHtml_();
      this.tooltip_ = this.computeTooltip_();
    }
  }

  private onActionClick_(e: MouseEvent|KeyboardEvent) {
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

  private computeActionIconStyle_(): string {
    // If this is a custom icon, shouldn't follow the standard theming given to
    // all other action icons.
    if (this.action.iconPath.startsWith('data:image/')) {
      return `background-image: url(${this.action.iconPath})`;
    } else {
      return `-webkit-mask-image: url(${this.action.iconPath})`;
    }
  }

  private computeAriaLabel_(): string {
    if (this.action.a11yLabel) {
      return decodeString16(this.action.a11yLabel);
    }
    return '';
  }

  private computeHintHtml_(): TrustedHTML {
    if (this.action.hint) {
      return sanitizeInnerHtml(decodeString16(this.action.hint));
    }
    return window.trustedTypes!.emptyHTML;
  }

  private computeTooltip_(): string {
    if (this.action.suggestionContents) {
      return decodeString16(this.action.suggestionContents);
    }
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-action': SearchboxActionElement;
  }
}

customElements.define(SearchboxActionElement.is, SearchboxActionElement);

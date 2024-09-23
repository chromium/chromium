// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_style.css.js';

import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './searchbox_action.html.js';
import type {Action} from './searchbox.mojom-webui.js';
import {decodeString16} from './utils.js';

// Displays an action associated with AutocompleteMatch (i.e. Clear
// Browsing History, etc.)
class SearchboxActionElement extends PolymerElement {
  static get is() {
    return 'cr-searchbox-action';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================
      action: {
        type: Object,
      },

      /**
       * Index of the action in the autocomplete result. Used to inform handler
       * of action that was selected.
       */
      actionIndex: {
        type: Number,
        value: -1,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as click, keyboard events etc.
       */
      matchIndex: {
        type: Number,
        value: -1,
      },

      //========================================================================
      // Private properties
      //========================================================================
      /** Element's 'aria-label' attribute. */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(action)`,
        reflectToAttribute: true,
      },

      /** Rendered hint from action. */
      hintHtml_: {
        type: String,
        computed: `computeHintHtml_(action)`,
      },

      /** Rendered tooltip from action. */
      tooltip_: {
        type: String,
        computed: `computeTooltip_(action)`,
      },
    };
  }

  action: Action;
  actionIndex: number;
  matchIndex: number;
  override ariaLabel: string;
  private hintHtml_: TrustedHTML;
  private tooltip_: string;

  override ready() {
    super.ready();

    this.addEventListener('click', (event) => this.onActionClick_(event));
    this.addEventListener('keydown', (event) => this.onActionKeyDown_(event));
    this.addEventListener(
        'mousedown', (event) => this.onActionMouseDown_(event));
  }

  private onActionClick_(e: MouseEvent|KeyboardEvent) {
    this.dispatchEvent(new CustomEvent('execute-action', {
      bubbles: true,
      composed: true,
      detail: {
        event: e,
        actionIndex: this.actionIndex,
      },
    }));

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

customElements.define(SearchboxActionElement.is, SearchboxActionElement);

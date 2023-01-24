// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox_icon.js';
import './realbox_action.js';
import './realbox_dropdown_shared_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ACMatchClassification, AutocompleteMatch, NavigationPredictor, PageHandlerInterface} from './omnibox.mojom-webui.js';
import {RealboxBrowserProxy} from './realbox_browser_proxy.js';
import {RealboxIconElement} from './realbox_icon.js';
import {getTemplate} from './realbox_match.html.js';
import {decodeString16, mojoTimeTicks} from './utils.js';



// clang-format off
/**
 * Bitmap used to decode the value of ACMatchClassification style
 * field.
 * See components/omnibox/browser/autocomplete_match.h.
 */
enum AcMatchClassificationStyle {
  NONE = 0,
  URL =   1 << 0,  // A URL.
  MATCH = 1 << 1,  // A match for the user's search term.
  DIM =   1 << 2,  // A "helper text".
}
// clang-format on

const ENTITY_MATCH_TYPE: string = 'search-suggest-entity';

export interface RealboxMatchElement {
  $: {
    icon: RealboxIconElement,
    contents: HTMLElement,
    description: HTMLElement,
    remove: HTMLElement,
    separator: HTMLElement,
    'focus-indicator': HTMLElement,
  };
}

// Displays an autocomplete match similar to those in the Omnibox.
export class RealboxMatchElement extends PolymerElement {
  static get is() {
    return 'cr-realbox-match';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /** Element's 'aria-label' attribute. */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(match.a11yLabel)`,
        reflectToAttribute: true,
      },

      /**
       * Whether the match features an image (as opposed to an icon or favicon).
       */
      hasImage: {
        type: Boolean,
        computed: `computeHasImage_(match)`,
        reflectToAttribute: true,
      },

      /**
       * Whether the match is an entity suggestion (with or without an image).
       */
      isEntitySuggestion: {
        type: Boolean,
        computed: `computeIsEntitySuggestion_(match)`,
        reflectToAttribute: true,
      },

      /**
       * Whether the match should be rendered in a two-row layout. Currently
       * limited to matches that feature an image, calculator, and answers.
       */
      isRichSuggestion: {
        type: Boolean,
        computed: `computeIsRichSuggestion_(match)`,
        reflectToAttribute: true,
      },

      match: {
        type: Object,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       */
      matchIndex: {
        type: Number,
        value: -1,
      },

      //========================================================================
      // Private properties
      //========================================================================

      actionIsVisible_: {
        type: Boolean,
        computed: `computeActionIsVisible_(match)`,
      },

      /** Rendered match contents based on autocomplete provided styling. */
      contentsHtml_: {
        type: String,
        computed: `computeContentsHtml_(match)`,
      },

      /** Rendered match description based on autocomplete provided styling. */
      descriptionHtml_: {
        type: String,
        computed: `computeDescriptionHtml_(match)`,
      },

      /** Remove button's 'aria-label' attribute. */
      removeButtonAriaLabel_: {
        type: String,
        computed: `computeRemoveButtonAriaLabel_(match.removeButtonA11yLabel)`,
      },

      removeButtonTitle_: {
        type: String,
        value: () => loadTimeData.getString('removeSuggestion'),
      },

      /** Used to separate the contents from the description. */
      separatorText_: {
        type: String,
        computed: `computeSeparatorText_(match)`,
      },

      /** Rendered tail suggest common prefix. */
      tailSuggestPrefix_: {
        type: String,
        computed: `computeTailSuggestPrefix_(match)`,
      },
    };
  }

  override ariaLabel: string;
  hasImage: boolean;
  match: AutocompleteMatch;
  matchIndex: number;
  private actionIsVisible_: boolean;
  private contentsHtml_: TrustedHTML;
  private descriptionHtml_: TrustedHTML;
  private removeButtonAriaLabel_: string;
  private removeButtonTitle_: string;
  private separatorText_: string;
  private tailSuggestPrefix_: string;

  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = RealboxBrowserProxy.getInstance().handler;
  }

  override ready() {
    super.ready();

    this.addEventListener('click', (event) => this.onMatchClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());
    this.addEventListener('mousedown', () => this.onMatchMouseDown_());
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * containing index of the match that was removed as well as modifier key
   * presses.
   */
  private executeAction_(e: MouseEvent|KeyboardEvent) {
    this.pageHandler_.executeAction(
        this.matchIndex, mojoTimeTicks(Date.now()),
        (e as MouseEvent).button || 0, e.altKey, e.ctrlKey, e.metaKey,
        e.shiftKey);
  }

  private onActionClick_(e: MouseEvent|KeyboardEvent) {
    this.executeAction_(e);

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onActionKeyDown_(e: KeyboardEvent) {
    if (e.key && (e.key === 'Enter' || e.key === ' ')) {
      this.onActionClick_(e);
    }
  }

  private onMatchClick_(e: MouseEvent) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    this.dispatchEvent(new CustomEvent('match-click', {
      bubbles: true,
      composed: true,
      detail: {index: this.matchIndex, event: e},
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onMatchFocusin_() {
    this.dispatchEvent(new CustomEvent('match-focusin', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));
  }

  private onMatchMouseDown_() {
    this.pageHandler_.onNavigationLikely(
        this.matchIndex, NavigationPredictor.kMouseDown);
  }

  private onRemoveButtonClick_(e: MouseEvent) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }
    this.dispatchEvent(new CustomEvent('match-remove', {
      bubbles: true,
      composed: true,
      detail: this.matchIndex,
    }));

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.
  }

  private onRemoveButtonMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================


  private computeAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return decodeString16(this.match.a11yLabel);
  }

  private sanitizeInnerHtml_(html: string): TrustedHTML {
    return sanitizeInnerHtml(html, {attrs: ['class']});
  }

  private computeContentsHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    const match = this.match;
    // `match.answer.firstLine` is generated by appending an optional additional
    // text from the answer's first line to `match.contents`, making the latter
    // a prefix of the former. Thus `match.answer.firstLine` can be rendered
    // using the markup in `match.contentsClass` which contains positions in
    // `match.contents` and the markup to be applied to those positions.
    // See //chrome/browser/ui/webui/realbox/realbox_handler.cc
    const matchContents =
        match.answer ? match.answer.firstLine : match.contents;
    return match.swapContentsAndDescription ?
        this.sanitizeInnerHtml_(
            this.renderTextWithClassifications_(
                    decodeString16(match.description), match.descriptionClass)
                .innerHTML) :
        this.sanitizeInnerHtml_(
            this.renderTextWithClassifications_(
                    decodeString16(matchContents), match.contentsClass)
                .innerHTML);
  }

  private computeDescriptionHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    const match = this.match;
    if (match.answer) {
      return this.sanitizeInnerHtml_(decodeString16(match.answer.secondLine));
    }
    return match.swapContentsAndDescription ?
        this.sanitizeInnerHtml_(
            this.renderTextWithClassifications_(
                    decodeString16(match.contents), match.contentsClass)
                .innerHTML) :
        this.sanitizeInnerHtml_(
            this.renderTextWithClassifications_(
                    decodeString16(match.description), match.descriptionClass)
                .innerHTML);
  }

  private computeTailSuggestPrefix_(): string {
    if (!this.match || !this.match.tailSuggestCommonPrefix) {
      return '';
    }
    const prefix = decodeString16(this.match.tailSuggestCommonPrefix);
    // Replace last space with non breaking space since spans collapse
    // trailing white spaces and the prefix always ends with a white space.
    if (prefix.slice(-1) === ' ') {
      return prefix.slice(0, -1) + '\u00A0';
    }
    return prefix;
  }

  private computeHasImage_(): boolean {
    return this.match && !!this.match.imageUrl;
  }

  private computeIsEntitySuggestion_(): boolean {
    return this.match && this.match.type === ENTITY_MATCH_TYPE;
  }

  private computeIsRichSuggestion_(): boolean {
    return this.match && this.match.isRichSuggestion;
  }

  private computeActionIsVisible_(): boolean {
    return this.match && !!this.match.action;
  }

  private computeRemoveButtonAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return decodeString16(this.match.removeButtonA11yLabel);
  }

  private computeSeparatorText_(): string {
    return this.match && decodeString16(this.match.description) ?
        loadTimeData.getString('realboxSeparator') :
        '';
  }

  /**
   * Decodes the AcMatchClassificationStyle enteries encoded in the given
   * ACMatchClassification style field, maps each entry to a CSS
   * class and returns them.
   */
  private convertClassificationStyleToCssClasses_(style: number): string[] {
    const classes = [];
    if (style & AcMatchClassificationStyle.DIM) {
      classes.push('dim');
    }
    if (style & AcMatchClassificationStyle.MATCH) {
      classes.push('match');
    }
    if (style & AcMatchClassificationStyle.URL) {
      classes.push('url');
    }
    return classes;
  }

  private createSpanWithClasses_(text: string, classes: string[]): Element {
    const span = document.createElement('span');
    if (classes.length) {
      span.classList.add(...classes);
    }
    span.textContent = text;
    return span;
  }

  /**
   * Renders |text| based on the given ACMatchClassification(s)
   * Each classification contains an 'offset' and an encoded list of styles for
   * styling a substring starting with the 'offset' and ending with the next.
   * @return A <span> with <span> children for each styled substring.
   */
  private renderTextWithClassifications_(
      text: string, classifications: ACMatchClassification[]): Element {
    return classifications
        .map(({offset, style}, index) => {
          const next = classifications[index + 1] || {offset: text.length};
          const subText = text.substring(offset, next.offset);
          const classes = this.convertClassificationStyleToCssClasses_(style);
          return this.createSpanWithClasses_(subText, classes);
        })
        .reduce((container, currentElement) => {
          container.appendChild(currentElement);
          return container;
        }, document.createElement('span'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-realbox-match': RealboxMatchElement;
  }
}

customElements.define(RealboxMatchElement.is, RealboxMatchElement);

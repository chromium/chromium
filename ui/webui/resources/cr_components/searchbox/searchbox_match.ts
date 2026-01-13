// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_icon.js';
import './searchbox_action.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from '//resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {ACMatchClassification, AutocompleteMatch, OmniboxPopupSelection, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {SelectionLineState, SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {createAutocompleteMatch, SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxIconElement} from './searchbox_icon.js';
import {getCss} from './searchbox_match.css.js';
import {getHtml} from './searchbox_match.html.js';
import {mojoTimeTicks} from './utils.js';



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

// Represents the initial selection when a match is created or reset.
const defaultSelection: OmniboxPopupSelection = {
  line: -1,
  state: SelectionLineState.kNormal,
  actionIndex: 0,
};

type ActionEvent = CustomEvent<{
  event: PointerEvent | KeyboardEvent,
  actionIndex: number,
}>;

export interface SearchboxMatchElement {
  $: {
    icon: SearchboxIconElement,
    contents: HTMLElement,
    description: HTMLElement,
    remove: HTMLElement,
    separator: HTMLElement,
    'focus-indicator': HTMLElement,
  };
}

// Displays an autocomplete match.
export class SearchboxMatchElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-match';
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

      /** Element's 'aria-label' attribute. */
      ariaLabel: {type: String},

      hasAction: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the match features an image (as opposed to an icon or favicon).
       */
      hasImage: {
        type: Boolean,
        reflect: true,
      },

      hasKeyword: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the match is an entity suggestion (with or without an image).
       */
      isEntitySuggestion: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the match should be rendered in a two-row layout. Currently
       * limited to matches that feature an image, calculator, and answers.
       */
      isRichSuggestion: {
        type: Boolean,
        reflect: true,
      },

      match: {type: Object},

      selection: {type: Object},

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as deletion, click, etc.
       */
      matchIndex: {type: Number},

      showThumbnail: {
        type: Boolean,
        reflect: true,
      },

      showEllipsis: {type: Boolean},
      sideType: {type: Number},

      //========================================================================
      // Private properties
      //========================================================================

      isTopChromeSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      forceHideEllipsis_: {type: Boolean},

      /** Rendered match contents based on autocomplete provided styling. */
      contentsHtml_: {type: String},

      /** Rendered match description based on autocomplete provided styling. */
      descriptionHtml_: {type: String},

      enableCsbMotionTweaks_: {
        type: Boolean,
        reflect: true,
      },

      /** Remove button's 'aria-label' attribute. */
      removeButtonAriaLabel_: {type: String},

      removeButtonTitle_: {type: String},

      /** Used to separate the contents from the description. */
      separatorText_: {type: String},

      /** Rendered tail suggest common prefix. */
      tailSuggestPrefix_: {type: String},
    };
  }

  override accessor ariaLabel: string = '';
  accessor hasAction: boolean = false;
  accessor hasImage: boolean = false;
  accessor hasKeyword: boolean = false;
  accessor isEntitySuggestion: boolean = false;
  accessor isRichSuggestion: boolean = false;
  accessor match: AutocompleteMatch = createAutocompleteMatch();
  accessor selection: OmniboxPopupSelection = defaultSelection;
  accessor matchIndex: number = -1;
  accessor sideType: SideType = SideType.kDefaultPrimary;
  accessor showThumbnail: boolean = false;
  accessor showEllipsis: boolean = false;
  private accessor isTopChromeSearchbox_: boolean =
      loadTimeData.getBoolean('isTopChromeSearchbox');
  private accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');
  private accessor forceHideEllipsis_: boolean =
      loadTimeData.getBoolean('forceHideEllipsis');
  protected accessor contentsHtml_: TrustedHTML =
      window.trustedTypes!.emptyHTML;
  protected accessor descriptionHtml_: TrustedHTML =
      window.trustedTypes!.emptyHTML;
  protected accessor enableCsbMotionTweaks_: boolean =
      loadTimeData.getBoolean('enableCsbMotionTweaks');
  protected accessor removeButtonAriaLabel_: string = '';
  protected accessor removeButtonTitle_: string =
      loadTimeData.getString('removeSuggestion');
  protected accessor separatorText_: string = '';
  protected accessor tailSuggestPrefix_: string = '';

  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
  }

  override firstUpdated() {
    this.addEventListener('click', (event) => this.onMatchClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());
    this.addEventListener('mousedown', () => this.onMatchMouseDown_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('match')) {
      this.ariaLabel = this.computeAriaLabel_();
      this.contentsHtml_ = this.computeContentsHtml_();
      this.descriptionHtml_ = this.computeDescriptionHtml_();
      this.hasAction = this.computeHasAction_();
      this.hasKeyword = this.computeHasKeyword_();
      this.hasImage = this.computeHasImage_();
      this.isEntitySuggestion = this.computeIsEntitySuggestion_();
      this.isRichSuggestion = this.computeIsRichSuggestion_();
      this.removeButtonAriaLabel_ = this.computeRemoveButtonAriaLabel_();
      this.separatorText_ = this.computeSeparatorText_();
      this.tailSuggestPrefix_ = this.computeTailSuggestPrefix_();
      this.selection = defaultSelection;
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('showThumbnail') ||
        changedPrivateProperties.has('isLensSearchbox_') ||
        changedPrivateProperties.has('forceHideEllipsis_')) {
      this.showEllipsis = this.computeShowEllipsis_();
    }
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onActivateKeyword_(e: ActionEvent) {
    // Keyboard activation isn't possible because when the keyword chip is
    // focused, focus is redirected to the omnibox view.
    const event = e.detail.event as PointerEvent;
    this.pageHandler_.activateKeyword(
        this.matchIndex, this.match.destinationUrl, mojoTimeTicks(Date.now()),
        // Distinguish mouse and touch or pen events for logging purposes.
        event.pointerType === 'mouse');
  }

  /**
   * containing index of the action that was removed as well as modifier key
   * presses.
   */
  protected onExecuteAction_(e: ActionEvent) {
    const event = e.detail.event;
    this.pageHandler_.executeAction(
        this.matchIndex, e.detail.actionIndex, this.match.destinationUrl,
        mojoTimeTicks(Date.now()), (event as MouseEvent).button || 0,
        event.altKey, event.ctrlKey, event.metaKey, event.shiftKey);
  }

  private onMatchClick_(e: MouseEvent) {
    if (e.button > 1) {
      // Only handle main (generally left) and middle button presses.
      return;
    }

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.

    this.pageHandler_.openAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl,
        /* are_matches_showing */ true, e.button || 0, e.altKey, e.ctrlKey,
        e.metaKey, e.shiftKey);

    // Duplicates the logic in `ui::DispositionFromClick()`.
    const backgroundTab = (e.metaKey || e.ctrlKey) && e.shiftKey;
    // 'match-click' event is used to close the dropdown. Don't do so when
    // opening a background tab so users can open multiple matches.
    if (!backgroundTab) {
      this.fire('match-click');
    }
  }

  private onMatchFocusin_() {
    this.fire('match-focusin', this.matchIndex);
  }

  private onMatchMouseDown_() {
    this.pageHandler_.onNavigationLikely(
        this.matchIndex, this.match.destinationUrl,
        NavigationPredictor.kMouseDown);
  }

  protected onRemoveButtonClick_(e: MouseEvent) {
    if (e.button !== 0) {
      // Only handle main (generally left) button presses.
      return;
    }

    e.preventDefault();   // Prevents default browser action (navigation).
    e.stopPropagation();  // Prevents <iron-selector> from selecting the match.

    this.pageHandler_.deleteAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl);
  }

  protected onRemoveButtonMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return this.match.a11yLabel;
  }

  /**
   * Sanitizes .innerHTML from `renderTextWithClassifications_()` through
   * `sanitizeInnerHtml` to ensure it only contains allowed tags.
   * @param innerHtml The .innerHTML from `renderTextWithClassifications_()`
   * @return Sanitized TrustedHTML safe for rendering
   */
  private sanitizeInnerHtml_(innerHtml: string): TrustedHTML {
    try {
      return sanitizeInnerHtml(innerHtml, {attrs: ['class']});
    } catch (e) {
      // If sanitization fails return empty HTML.
      return window.trustedTypes!.emptyHTML;
    }
  }

  private computeContentsHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    // `match.answer.firstLine` is generated by appending an optional additional
    // text from the answer's first line to `match.contents`, making the latter
    // a prefix of the former. Thus `match.answer.firstLine` can be rendered
    // using the markup in `match.contentsClass` which contains positions in
    // `match.contents` and the markup to be applied to those positions.
    // See //chrome/browser/ui/webui/searchbox/searchbox_handler.cc
    return this.sanitizeInnerHtml_(
        this.renderTextWithClassifications_(
                this.getMatchContents_(),
                this.getMatchContentsClassifications_())
            .innerHTML);
  }

  private computeDescriptionHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    return this.sanitizeInnerHtml_(
        this.renderTextWithClassifications_(
                this.getMatchDescription_(),
                this.match.answer ? [] :
                                    this.getMatchDescriptionClassifications_())
            .innerHTML);
  }

  private computeHasAction_() {
    return this.match?.actions?.length > 0;
  }

  private computeHasKeyword_(): boolean {
    return this.match && !!this.match.keywordChipHint;
  }

  private computeHasImage_(): boolean {
    return this.match && !!this.match.imageUrl;
  }

  private computeIsEntitySuggestion_(): boolean {
    return this.match && this.match.type === ENTITY_MATCH_TYPE;
  }

  private computeIsRichSuggestion_(): boolean {
    // When the searchbox is embedded in the top-chrome (i.e. Omnibox), all
    // suggestions should be rendered using a one-line layout.
    return !this.isTopChromeSearchbox_ && this.match &&
        this.match.isRichSuggestion;
  }

  private computeRemoveButtonAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return this.match.removeButtonA11yLabel;
  }

  private computeSeparatorText_(): string {
    return this.getMatchDescription_() ?
        loadTimeData.getString('searchboxSeparator') :
        '';
  }

  private computeTailSuggestPrefix_(): string {
    if (!this.match || !this.match.tailSuggestCommonPrefix) {
      return '';
    }
    const prefix = this.match.tailSuggestCommonPrefix;
    // Replace last space with non breaking space since spans collapse
    // trailing white spaces and the prefix always ends with a white space.
    if (prefix.slice(-1) === ' ') {
      return prefix.slice(0, -1) + '\u00A0';
    }
    return prefix;
  }

  private computeShowEllipsis_(): boolean {
    if (this.isLensSearchbox_ && this.forceHideEllipsis_) {
      return false;
    }
    return this.showThumbnail;
  }

  /**
   * Decodes the AcMatchClassificationStyle entries encoded in the given
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
    const container = document.createElement('span');

    // If no classifications are provided, render the entire text unstyled.
    if (classifications.length === 0) {
      container.appendChild(this.createSpanWithClasses_(text, []));
      return container;
    }

    // If the first classification doesn't start at 0, render the prefix text
    // unstyled. `AutocompleteMatch::ValidateClassifications()` guarantees the
    // first offset is 0, however this is only validated in debug builds.
    const firstClassification = classifications[0]!;
    if (firstClassification.offset > 0) {
      const prefix = text.substring(0, firstClassification.offset);
      container.appendChild(this.createSpanWithClasses_(prefix, []));
    }

    classifications.map(({offset, style}, index) => {
      // Each classification defines a region from its offset to the next
      // classification's offset or end of string for the last one. This covers
      // the entire string with no gaps.
      const nextOffset = index + 1 < classifications.length ?
          classifications[index + 1]!.offset :
          text.length;
      const subString = text.substring(offset, nextOffset);
      const classes = this.convertClassificationStyleToCssClasses_(style);
      container.appendChild(this.createSpanWithClasses_(subString, classes));
    });

    return container;
  }

  private getMatchContents_(): string {
    if (!this.match) {
      return '';
    }

    const match = this.match;
    const matchContents =
        match.answer ? match.answer.firstLine : match.contents;
    const matchDescription =
        match.answer ? match.answer.secondLine : match.description;

    return match.swapContentsAndDescription ? matchDescription : matchContents;
  }

  private getMatchDescription_(): string {
    if (!this.match) {
      return '';
    }

    const match = this.match;
    const matchContents =
        match.answer ? match.answer.firstLine : match.contents;
    const matchDescription =
        match.answer ? match.answer.secondLine : match.description;

    return match.swapContentsAndDescription ? matchContents : matchDescription;
  }

  private getMatchContentsClassifications_(): ACMatchClassification[] {
    if (!this.match) {
      return [];
    }
    const match = this.match;
    return match.swapContentsAndDescription ? match.descriptionClass :
                                              match.contentsClass;
  }

  private getMatchDescriptionClassifications_(): ACMatchClassification[] {
    if (!this.match) {
      return [];
    }
    const match = this.match;
    return match.swapContentsAndDescription ? match.contentsClass :
                                              match.descriptionClass;
  }

  protected getFocusIndicatorCssClass_(): string {
    return this.selection.line === this.matchIndex &&
            this.selection.state !== SelectionLineState.kNormal &&
            !this.match.hasInstantKeyword ?
        'selected-within' :
        '';
  }

  protected getKeywordCssClass_(): string {
    return this.selection.line === this.matchIndex &&
            this.selection.state === SelectionLineState.kKeywordMode ?
        'selected' :
        '';
  }

  protected getActionCssClass_(actionIndex: number): string {
    return this.selection.line === this.matchIndex &&
            this.selection.state === SelectionLineState.kFocusedButtonAction &&
            this.selection.actionIndex === actionIndex ?
        'selected' :
        '';
  }

  protected getRemoveCssClass_(): string {
    return this.selection.line === this.matchIndex &&
            this.selection.state ===
                SelectionLineState.kFocusedButtonRemoveSuggestion ?
        'selected' :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-match': SearchboxMatchElement;
  }
}

customElements.define(SearchboxMatchElement.is, SearchboxMatchElement);

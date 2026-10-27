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
import type {AutocompleteMatch, OmniboxPopupSelection, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {KeywordActivationMethod, KeywordType, SelectionLineState, SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {FormattedString, FormattedStringFragment} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import {ColorType, SecondaryTextPlacement} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';

import {createAutocompleteMatch, SearchboxBrowserProxy} from './searchbox_browser_proxy.js';
import type {SearchboxIconElement} from './searchbox_icon.js';
import {getCss} from './searchbox_match.css.js';
import {getHtml} from './searchbox_match.html.js';
import {mojoTimeTicks} from './utils.js';



const ENTITY_MATCH_TYPE: string = 'search-suggest-entity';

// Represents the initial selection when a match is created or reset.
export const kDefaultSelection: OmniboxPopupSelection = {
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
    primaryText: HTMLElement,
    secondaryText: HTMLElement,
    remove: HTMLElement,
    separator: HTMLElement,
    focusIndicator: HTMLElement,
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
      ariaLabel: {
        type: String,
        reflect: true,
      },

      hasAction: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the secondary text is only shown while the match is selected
       * or hovered.
       */
      showSecondaryTextOnlyOnSelection: {
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

      hasKeywordChip: {
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

      match: {type: Object},

      /** Where the secondary text is rendered relative to the primary text. */
      secondaryTextPlacement: {
        type: String,
        reflect: true,
        attribute: 'secondary-text-placement',
      },

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

      virtualFocusEnabled: {type: Boolean},
      resultSequenceId: {type: Number},

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * Whether the secondary text is a URL, e.g. a page title followed by its
       * URL. The URL layout rules key on it.
       */
      secondaryTextIsUrl_: {
        type: Boolean,
        reflect: true,
      },

      isTopChromeSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      isLensSearchbox_: {
        type: Boolean,
        reflect: true,
      },

      forceHideEllipsis_: {type: Boolean},

      /** Rendered primary text based on autocomplete provided styling. */
      primaryTextHtml_: {type: String},

      /** Rendered secondary text based on autocomplete provided styling. */
      secondaryTextHtml_: {type: String},

      enableCsbMotionTweaks_: {
        type: Boolean,
        reflect: true,
      },

      /** Remove button's 'aria-label' attribute. */
      removeButtonAriaLabel_: {type: String},

      removeButtonTitle_: {type: String},

      /** Used to separate the primary text from the secondary text. */
      separatorText_: {type: String},

      /** Rendered tail suggest common prefix. */
      tailSuggestPrefix_: {type: String},
    };
  }

  override accessor ariaLabel: string = '';
  accessor hasAction: boolean = false;
  accessor showSecondaryTextOnlyOnSelection: boolean = false;
  accessor hasImage: boolean = false;
  accessor hasKeywordChip: boolean = false;
  accessor isEntitySuggestion: boolean = false;
  accessor match: AutocompleteMatch = createAutocompleteMatch();
  accessor secondaryTextPlacement: string = 'in-front-of-primary-text';
  accessor selection: OmniboxPopupSelection = kDefaultSelection;
  accessor matchIndex: number = -1;
  accessor sideType: SideType = SideType.kDefaultPrimary;
  accessor showThumbnail: boolean = false;
  accessor showEllipsis: boolean = false;
  accessor virtualFocusEnabled: boolean = false;
  accessor resultSequenceId: number = 0;
  private accessor secondaryTextIsUrl_: boolean = false;
  private accessor isTopChromeSearchbox_: boolean =
      loadTimeData.getBoolean('isTopChromeSearchbox');
  private accessor isLensSearchbox_: boolean =
      loadTimeData.getBoolean('isLensSearchbox');
  private accessor forceHideEllipsis_: boolean =
      loadTimeData.getBoolean('forceHideEllipsis');
  protected accessor primaryTextHtml_: TrustedHTML =
      window.trustedTypes!.emptyHTML;
  protected accessor secondaryTextHtml_: TrustedHTML =
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('match')) {
      this.ariaLabel = this.computeAriaLabel_();
      this.primaryTextHtml_ = this.computePrimaryTextHtml_();
      this.secondaryTextHtml_ = this.computeSecondaryTextHtml_();
      this.hasAction = this.computeHasAction_();
      this.hasKeywordChip = this.computeHasKeywordChip_();
      this.hasImage = this.computeHasImage_();
      this.showSecondaryTextOnlyOnSelection =
          this.match.showSecondaryTextOnlyOnSelection;
      this.isEntitySuggestion = this.computeIsEntitySuggestion_();
      this.secondaryTextPlacement = this.computeSecondaryTextPlacement_();
      this.secondaryTextIsUrl_ = this.match.secondaryTextIsUrl;
      this.removeButtonAriaLabel_ = this.computeRemoveButtonAriaLabel_();
      this.separatorText_ = this.computeSeparatorText_();
      this.tailSuggestPrefix_ = this.computeTailSuggestPrefix_();
      this.selection = kDefaultSelection;
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('showThumbnail') ||
        changedPrivateProperties.has('isLensSearchbox_') ||
        changedPrivateProperties.has('forceHideEllipsis_')) {
      this.showEllipsis = this.computeShowEllipsis_();
    }
  }

  override firstUpdated() {
    this.addEventListener('click', (event) => this.onMatchClick_(event));
    this.addEventListener('auxclick', (event) => this.onMatchClick_(event));
    this.addEventListener('focusin', () => this.onMatchFocusin_());
    this.addEventListener(
        'mousedown', (event) => this.onMatchMouseDown_(event));
  }

  // Note: the `aria-label` is the match's own label and does not follow the
  // virtual focus selection. Virtual focus selection changes, including those
  // onto this match's buttons, are narrated by the hosting searchbox (see
  // `SearchboxMixin.narrateSelection()`), so this match must not announce them
  // too, by any means.

  //============================================================================
  // Event handlers
  //============================================================================

  protected onKeywordExecuteAction_(e: ActionEvent) {
    // Keyboard activation isn't possible because when the keyword chip is
    // focused, focus is redirected to the omnibox view.
    const event = e.detail.event as PointerEvent;
    this.fire(
        'keyword-click', {match: this.match, matchIndex: this.matchIndex});
    this.pageHandler_.activateKeyword(
        this.matchIndex, this.match.destinationUrl, mojoTimeTicks(Date.now()),
        // Distinguish mouse and touch or pen events for logging purposes.
        event.pointerType === 'mouse' ? KeywordActivationMethod.kClick :
                                        KeywordActivationMethod.kTap);
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

    if (this.match.keywordModel?.type === KeywordType.kInstant) {
      this.fire(
          'keyword-click', {match: this.match, matchIndex: this.matchIndex});
      this.pageHandler_.activateKeyword(
          this.matchIndex, this.match.destinationUrl, mojoTimeTicks(Date.now()),
          e.button === 0 ? KeywordActivationMethod.kClick :
                           KeywordActivationMethod.kTap);
      return;
    }

    this.pageHandler_.openAutocompleteMatch(
        this.resultSequenceId, this.matchIndex, this.match.destinationUrl,
        /*areMatchesShowing=*/ true,
        /*mouseButton=*/ e.button || 0, {
          altKey: e.altKey,
          ctrlKey: e.ctrlKey,
          metaKey: e.metaKey,
          shiftKey: e.shiftKey,
        },
        /*viaKeyboard=*/ false);

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

  private onMatchMouseDown_(e: MouseEvent) {
    if (this.virtualFocusEnabled ||
        this.match.keywordModel?.type === KeywordType.kInstant) {
      e.preventDefault();  // Prevents default browser action (focus loss).
    }
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

    this.fire('match-remove', this.matchIndex);
    this.pageHandler_.deleteAutocompleteMatch(
        this.matchIndex, this.match.destinationUrl);
  }

  protected onRemoveButtonMousedown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================

  private computeAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    // `a11yLabel` is the complete label, computed in C++ with the same logic
    // used by the omnibox edit model. It already includes the description
    // for match types where it is relevant.
    return this.match.a11yLabel;
  }

  /**
   * Sanitizes .innerHTML from `renderFormattedString_()` through
   * `sanitizeInnerHtml` to ensure it only contains allowed tags.
   * @param innerHtml The .innerHTML from `renderFormattedString_()`
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

  private computePrimaryTextHtml_(): TrustedHTML {
    if (!this.match) {
      return window.trustedTypes!.emptyHTML;
    }
    // See //components/omnibox/browser/suggest_template_info_mojo_utils.cc
    return this.sanitizeInnerHtml_(
        this.renderFormattedString_(this.match.suggestTemplate.primaryText)
            .innerHTML);
  }

  private computeSecondaryTextHtml_(): TrustedHTML {
    const secondaryText = this.match?.suggestTemplate.secondaryText;
    if (!secondaryText) {
      return window.trustedTypes!.emptyHTML;
    }
    return this.sanitizeInnerHtml_(
        this.renderFormattedString_(secondaryText).innerHTML);
  }

  private computeHasAction_() {
    return this.match?.actions?.length > 0;
  }

  private computeHasKeywordChip_(): boolean {
    return this.match?.keywordModel?.type === KeywordType.kChip;
  }

  private computeHasImage_(): boolean {
    return this.match && !!this.match.suggestTemplate.image?.url;
  }

  private computeIsEntitySuggestion_(): boolean {
    return this.match && this.match.type === ENTITY_MATCH_TYPE;
  }

  private computeSecondaryTextPlacement_(): string {
    // When the searchbox is embedded in the top-chrome (i.e. Omnibox), all
    // suggestions should be rendered using a one-line layout.
    // TODO(crbug.com/568747976): Decide this in C++ along with the rest of the
    // placement (see suggest_template_info_mojo_utils.cc), which requires the
    // handler to know whether it backs the top-chrome searchbox.
    if (!this.isTopChromeSearchbox_ && this.match &&
        this.match.suggestTemplate.secondaryTextPlacement ===
            SecondaryTextPlacement.kBelowPrimaryText) {
      return 'below-primary-text';
    }
    return 'in-front-of-primary-text';
  }

  private computeRemoveButtonAriaLabel_(): string {
    if (!this.match) {
      return '';
    }
    return this.match.removeButtonA11yLabel;
  }

  private computeSeparatorText_(): string {
    return this.getMatchSecondaryText_() ?
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
   * Maps the color and weight of a FormattedString fragment to CSS classes.
   * Each suggest color gets a class named after the color token it maps to.
   * Other colors get no class and keep the default color.
   */
  private getFragmentCssClasses_(fragment: FormattedStringFragment): string[] {
    const classes = [];
    switch (fragment.color) {
      case ColorType.kSuggestQueryText:
        classes.push('color-on-surface');
        break;
      case ColorType.kSuggestHintText:
        classes.push('color-on-surface-subtle');
        break;
      case ColorType.kSuggestActionHigh:
        classes.push('color-primary');
        break;
      default:
        break;
    }
    if (fragment.isBolded) {
      classes.push('match');
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
   * Renders the text of `formattedString` based on its fragments. Each
   * fragment styles the text from its `startIndex` up to the next fragment's
   * `startIndex`, or to the end of the text for the last one.
   * @return A <span> with <span> children for each styled substring.
   */
  private renderFormattedString_(formattedString: FormattedString): Element {
    const {text, fragments} = formattedString;
    const container = document.createElement('span');

    // If no fragments are provided, render the entire text unstyled.
    if (fragments.length === 0) {
      container.appendChild(this.createSpanWithClasses_(text, []));
      return container;
    }

    // If the first fragment doesn't start at 0, render the prefix text
    // unstyled.
    const firstFragment = fragments[0]!;
    if (firstFragment.startIndex > 0) {
      const prefix = text.substring(0, firstFragment.startIndex);
      container.appendChild(this.createSpanWithClasses_(prefix, []));
    }

    fragments.forEach((fragment, index) => {
      const nextStartIndex = index + 1 < fragments.length ?
          fragments[index + 1]!.startIndex :
          text.length;
      const subString = text.substring(fragment.startIndex, nextStartIndex);
      const classes = this.getFragmentCssClasses_(fragment);
      container.appendChild(this.createSpanWithClasses_(subString, classes));
    });

    return container;
  }

  private getMatchSecondaryText_(): string {
    return this.match?.suggestTemplate.secondaryText?.text ?? '';
  }

  protected getFocusIndicatorCssClass_(): string {
    return this.selection.line === this.matchIndex &&
            this.selection.state !== SelectionLineState.kNormal &&
            this.match.keywordModel?.type !== KeywordType.kInstant ?
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
